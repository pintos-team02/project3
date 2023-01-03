# PROJECT 1 : THREAD

## TOPICS
- Alarm clock

- Priority scheduling

- Advanced scheduler

## 일별 진행목록
||진행 사항|
|------|---|
|**금**|일정 수립, repo 생성, 이론 학습(스케줄링)|
|**토**|동료 학습(스케줄링), 이론 학습(쓰레드 : concurrency and thread)|
|**일**|이론 학습(쓰레드 : synchronization), 동료 학습(쓰레드), 구현 계획|
|**월**|alarm clock 구현|
|**화**|priority scheduling 구현|
|**수**|priority inversion problem|

## 구현내용 회고
### Trouble Shooting
#### `donate_priority`

> 가독성 좋은 코드의 중요성을 깨달았다.
> 
> 디버그의 비용은 비싸다.

<details>
  <summary>Before</summary>

  ```c
  void donate_priority(void)
  {
      struct thread *curr_thread = thread_current();
      struct lock *curr_thread_wait_lock = curr_thread->wait_on_lock;
      struct thread *lock_holder = curr_thread_wait_lock->holder;
      struct list lock_waiters = curr_thread_wait_lock->semaphore.waiters;
      int cnt = 0;

      while (curr_thread->priority > lock_holder->priority
              && lock_holder != NULL)
      {
          if (++cnt >= 8)
          {
              break;
          }

          lock_holder->priority = curr_thread->priority;
          curr_thread = lock_holder;
          lock_holder = lock_holder->wait_on_lock->holder;
      }
  }
  ```

- PPT 자료의 문장 그대로 구현

- 세부사항에 대한 고민 부족

- 코드의 가독성이 떨어짐.

</details>

<details>
  <summary>After</summary>

  ```c
  void donate_priority(void)
  {
      struct thread *curr_thread = thread_current();
      int cnt = 0;

      for (cnt = 0; cnt < 8; cnt++)
      {
          if (curr_thread->wait_on_lock == NULL)
              break;
          struct thread *lock_holder = curr_thread->wait_on_lock->holder;
          if (curr_thread->priority > lock_holder->priority)
          {
              lock_holder->priority = curr_thread->priority;
              curr_thread = lock_holder;
          }
      }
  }
  ```

- 가독성 고려하여 코드 개선

- 디버깅을 통한 흐름 이해

</details>

#### `remove_with_lock`

> 동료들은 다른 시선으로 볼 수 있다.
> 
> 포인터와 메모리 구조에 대해 빠삭하게 공부하자.

<details>
  <summary>Before</summary>

  ```c
  void remove_with_lock(struct lock *lock)
  {
      struct list lock_waiters = thread_current()->donations;
      struct list_elem *e;


      for (e = list_begin(&lock_waiters); e != list_end(&lock_waiters); e = list_next(e))
      {
          struct thread *lock_waiter = list_entry(e, struct thread, d_elem);

          if (lock == lock_waiter->wait_on_lock)
          {
              list_remove(&lock_waiter->d_elem);
              break;
          }
      }
  }
  ```

- 로직의 오류가 코드에 그대로 반영됨. (`break`문)

- 구조체 대입 연산에 대한 이해 부족

</details>

<details>
  <summary>After</summary>

  ```c
  void remove_with_lock(struct lock *lock)
  {
      struct thread *curr_thread = thread_current();
      struct list_elem *e;

      for (e = list_begin(&curr_thread->donations); e != list_end(&curr_thread->donations); e = list_next(e))
      {
          struct thread *lock_waiter = list_entry(e, struct thread, d_elem);

          if (lock == lock_waiter->wait_on_lock)
          {
              list_remove(&lock_waiter->d_elem);
          }
      }
  }
  ```

  또는

  ```c
  void remove_with_lock(struct lock *lock)
	{
      struct list *lock_waiters = &thread_current()->donations;
      struct list_elem *e;


      for (e = list_begin(lock_waiters); e != list_end(lock_waiters); e = list_next(e))
      {
          struct thread *lock_waiter = list_entry(e, struct thread, d_elem);

          if (lock == lock_waiter->wait_on_lock)
          {
              list_remove(&lock_waiter->d_elem);
          }
      }
  }
  ```

- 동료들과의 회의로 로직의 오류를 고쳐잡고, 코드 개선

- 디버깅

</details>

#### `thread_set_priority`

<details>
  <summary>Before</summary>

  ```c
  void thread_set_priority(int new_priority)
  {
      thread_current()->priority = new_priority;
      list_sort(&ready_list, cmp_priority, NULL);
      test_max_priority();
  }
  ```

- 해당 함수의 역할에 대한 이해 부족

</details>

<details>
  <summary>After</summary>

  ```c
  void thread_set_priority(int new_priority)
  {
      thread_current()->old_priority = new_priority;
      refresh_priority();
      test_max_priority();
  }
  ```

- 디버깅 (테스트 코드 확인 등)

</details>

#### `sema_up`

<details>
  <summary>Before</summary>

  ```c
  void sema_up (struct semaphore *sema) {
      enum intr_level old_level;

      ASSERT (sema != NULL);

      old_level = intr_disable ();
      if (!list_empty (&sema->waiters))
          thread_unblock (list_entry (list_pop_front (&sema->waiters),
                      struct thread, elem));
      sema->value++;
      list_sort(&sema->waiters, cmp_priority, NULL);
      test_max_priority();
      intr_set_level (old_level);
  }
  ```

  - 정렬이 안됐다.

</details>

<details>
  <summary>After</summary>

  ```c
  void sema_up(struct semaphore *sema)
  {
      enum intr_level old_level;

      ASSERT(sema != NULL);

      old_level = intr_disable();
      if (!list_empty(&sema->waiters))
      {
          list_sort(&sema->waiters, cmp_priority, NULL);
          thread_unblock(list_entry(list_pop_front(&sema->waiters),
                                  struct thread, elem));
      }
      sema->value++;
      test_max_priority();
      intr_set_level(old_level);
  }
  ```

- 디버깅

- 정렬을 해줬다.

</details>

#### `cond_var`

<details>
  <summary>코드</summary>

  ```c
  bool cmp_sem_priority(const struct list_elem *a, const struct list_elem *b, void *aux)
  {
      struct semaphore_elem *sema_a = list_entry(a, struct semaphore_elem, elem);
      struct semaphore_elem *sema_b = list_entry(b, struct semaphore_elem, elem);

      struct list *a_waiters = &(sema_a->semaphore.waiters);
      struct list *b_waiters = &(sema_b->semaphore.waiters);

      return list_entry(list_begin(a_waiters), struct thread, elem)->priority
              > list_entry(list_begin(b_waiters), struct thread, elem)->priority;
  }
  ```

- 리스트 엔트리에 대한 이해 부족

- 레퍼런스를 통해 함수 사용 방법을 이해하게 됨.

</details>

### 느낀 점

- 디버그의 필요성

- 잦은 커밋

- 학습 방식의 변화

- 메서드의 역할에 대한 이해 필요

- 테스트를 통과하기 위해서 주먹구구식으로 구현해나가는 방식에 대한 아쉬움

- 문제가 생겼을 때 로직에 대한 고민 필요

- C언어와 메모리 공간에 대한 명확한 이해
