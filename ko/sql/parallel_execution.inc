.. _parallel-execution:

병렬 처리
========================================

CUBRID는 대용량 데이터를 효율적으로 처리하기 위해 쿼리 수행 작업을 여러 작업자(worker)가 동시에 수행하는 병렬 처리를 지원한다.

각 작업자는 테이블의 서로 다른 페이지를 독립적으로 처리한 후, 그 결과를 통합하는 방식으로 동작한다.

이를 통해 전체 질의 실행 시간을 단축할 수 있다.

.. rubric:: SQL 힌트

병렬 처리를 사용하려면 다음과 같은 SQL 힌트를 사용해야 한다.

  .. code-block:: sql

    /*+ PARALLEL(N) */

여기서 N은 병렬 처리에 사용할 작업자의 수를 지정한다.
예를 들어, **PARALLEL(4)** 힌트는 4개의 작업자를 사용하여 질의를 병렬 처리한다.

.. rubric:: 시스템 파라미터

CUBRID는 병렬 처리에 사용할 수 있는 작업자의 최대 개수를 시스템 파라미터로 제한할 수 있다.
**max_parallel_workers** 파라미터는 동시에 사용 가능한 병렬 작업자의 최대 개수를 지정하며, 기본값은 32로 설정되어 있다.

이 파라미터는 시스템의 리소스 사용량에 직접적인 영향을 주지 않는다. 병렬 작업자가 실제로 사용되지 않을 때는
스레드가 종료된 상태로 관리되기 때문에, CPU나 메모리 자원을 점유하지 않는다.

.. rubric:: 예시

다음은 **max_parallel_workers** 파라미터가 어떻게 동작하는지 보여주는 예시이다:

* **max_parallel_workers=32** 인 경우:
    * **PARALLEL(20)** 힌트가 붙은 쿼리는 20개의 작업자를 예약한다.
    * 이 경우, 동일한 시점에 실행되는 다른 **PARALLEL(20)** 쿼리는 실행될 수 없다.
    * 왜냐하면 두 쿼리가 필요로 하는 작업자 수(20 + 20 = 40)가 **max_parallel_workers** 값(32)을 초과하기 때문이다.
    * 따라서 두 번째 쿼리는 단일 스레드로 처리되며, 첫 번째 쿼리가 완료될 때까지 남은 12개의 작업자만 사용할 수 있다.

.. _parallel-execution_heap-scan:

병렬 힙 스캔
----------------------------------------

병렬 힙 스캔(parallel heap scan)은 테이블의 데이터 페이지를 여러 작업자가 동시에 읽어 처리하는 방식이다.
각 작업자는 서로 다른 페이지의 데이터를 독립적으로 처리하며, 처리가 완료되면 결과를 통합한다.

.. rubric:: 기술적 지원 조건

CUBRID는 다음과 같은 경우에는 병렬 힙 스캔을 지원한다.

  1. 단순 테이블 전체 스캔
  2. JOIN 구문에서 첫 번째 테이블이 힙 스캔을 수행하는 경우
  3. 상관 관계가 없는 서브쿼리
  4. CTE의 비재귀 부분(non_recursive_part)

.. rubric:: 기술적 제한 사항

다음과 같은 경우에는 병렬 힙 스캔이 기술적으로 지원되지 않는다.

  1. 힙 스캔이 아닌 접근 방식 사용 시(예: 인덱스 스캔)
  2. CTE의 재귀 절이나 상관 부질의를 포함되는 경우
  3. CONNECT BY를 포함하는 질의의 경우
  4. incr(), decr() 함수 등을 클릭 카운터 함수를 포함한 질의의 경우
  5. PL/CSQL, 자바 저장 프로시저가 WHERE 절 등 조건절에 포함된 경우 (SELECT 목록에 포함된 경우는 지원)
  6. UPDATE, DELETE 등의 DML 문 (INSERT ~ SELECT 문은 지원)
  7. JOIN 구문에서 선행 테이블이 아닌 경우
  8. 시스템 카탈로그나 시스템 클래스에 접근하는 경우
  9. SELECT FOR UPDATE 구문(SELECT 잠금 필요)
  10. 테이블에서 점유하는 페이지 수가 32를 넘지 않는 경우

.. rubric:: 성능 관점에서 권장되는 경우

기술적으로 지원되는 경우라도, 다음 조건을 모두 만족할 때 병렬 힙 스캔을 사용하면 성능상 이점이 있다.

  1. 테이블의 크기가 충분히 큰 경우
  2. 시스템의 CPU 코어 수가 충분한 경우
  3. 충분한 메모리가 사용 가능한 경우
  4. WHERE 절의 조건이 데이터를 많이 걸러내거나, LIKE와 같은 처리 비용이 큰 연산을 포함하는 경우

.. note::

  병렬 처리는 시스템 리소스를 많이 사용하므로, 적절한 상황에서만 사용하는 것이 좋다.

  각 작업자가 처리한 데이터를 임시 볼륨과 메모리에 저장하기 때문에, 단일 처리 대비 더 많은 메모리를 사용한다.

.. rubric:: 병렬 힙 스캔 비활성화 방법

다음과 같은 방법으로 병렬 힙 스캔을 명시적으로 비활성화할 수 있다.

  1. SQL 힌트 사용:

     .. code-block:: sql

       /*+ NO_PARALLEL_HEAP_SCAN */

  2. 서버 파라미터 설정:

     .. code-block:: sql

       /* cubrid.conf */
       parallel_heap_scan_threads=0
       max_parallel_workers=0

.. _example_parallel-execution_heap-scan:

.. rubric:: 예시 및 프로파일링 결과

다음은 병렬 처리를 사용하여 힙 스캔을 수행하는 예시이다:

  .. code-block:: sql

    /* 4개의 작업자를 사용하여 병렬로 힙 스캔 수행 */
    SELECT /*+ PARALLEL(4) */ count(*) 
    FROM tmp 
    WHERE col1 like '%1010%' 
    AND col3 like '%10101%';

질의 프로파일링 결과에서도 병렬 처리가 어떻게 이루어졌는지 확인할 수 있다:

  .. code-block:: none

    Query Plan:
      TABLE SCAN (dba.tmp_1)

      rewritten query: select /*+ PARALLEL(4) */ count(*) from [dba.tmp_1] [dba.tmp_1] where [dba.tmp_1].col1 like  ?:0  and [dba.tmp_1].col3 like  ?:1 

    Trace Statistics:
      SELECT (time: 2724, fetch: 123458, fetch_time: 1375, ioread: 121729)
        SCAN (table: dba.tmp_1), (heap time: 2724, fetch: 123457, ioread: 121729, readrows: 299, rows: 299)
             (parallel workers: 4, heap time: 2723..2723, readrows: 2456794..2529468, rows: 22..131, gather: row by row)

프로파일링 결과를 통해 다음과 같은 정보를 확인할 수 있다:

  1. 각 작업자의 처리 결과:
      - 가장 많은 row를 읽은 작업자 : 2529468 row를 읽음
      - 가장 적은 row를 읽은 작업자 : 2456794 row를 읽음
      - 가장 많은 결과 row를 반환한 작업자 : 131 row를 반환함
      - 가장 적은 결과 row를 반환한 작업자 : 22 row를 반환함
      - 결과 병합 방식 : row by row
  2. 전체 처리 결과:
      - 총 실행 시간: 2,724ms
      - 총 fetch 횟수: 123,458회
      - 총 I/O 읽기 횟수: 121,729회
      - 최종 결과 레코드 수: 299개 (각 작업자의 결과를 병합)

이 예시에서는 약 1천만 개의 레코드(각 작업자가 약 250만 개씩 처리)를 읽어서 WHERE 절의 조건으로 299개의 레코드만 선택되었다.

이는 WHERE 절의 조건으로 많은 데이터를 걸러낼 수 있는 경우로, 병렬 힙 스캔이 효과적으로 동작하는 이상적인 상황을 보여준다.

.. rubric:: 지원 되지 않는 질의 예시

다음은 병렬 힙 스캔이 지원되지 않는 각 경우에 대한 구체적인 예시이다:

  1. 힙 스캔이 아닌 접근 방식 사용 시(예: 인덱스 스캔):
   .. code-block:: sql

    create table t (i int);
    create index idx on t (i);
    insert into t values (1);

    select /*+ recompile */ * from t where i = 1;
    
    Trace Statistics:
      SELECT (time: 0, fetch: 2, fetch_time: 0, ioread: 0)
        SCAN (index: dba.t.idx), (btree time: 0, fetch: 1, ioread: 0, readkeys: 1, filteredkeys: 0, rows: 1, covered: true)
  인덱스 스캔을 통해 접근하여, 병렬 힙 스캔이 수행되지 않는 예시이다.

  2. CTE의 재귀 절이나 상관 부질의를 포함되는 경우:
   .. code-block:: sql

    with recursive cte as (
      select col1, col2 from tbl where col1 < 100
      union all
      select col1 + 1, col2 + 1 from cte where col1 < 100
    )
    select count(*) from cte;

    Trace Statistics:
      SELECT (time: 8, fetch: 180, fetch_time: 0, ioread: 0)
        SCAN (temp time: 1, fetch: 23, ioread: 0, readrows: 8950, rows: 8950)
        SUBQUERY (uncorrelated)
          CTE (non_recursive_part)
            SELECT (time: 4, fetch: 80, fetch_time: 0, ioread: 0)
              SCAN (table: dba.tbl), (heap time: 4, fetch: 79, ioread: 0, readrows: 4000, rows: 4000)
                   (parallel workers: 2, heap time: 2..2, readrows: 1999..2001, rows: 1999..2001, gather: mergable list)
          CTE (recursive_part)
            SELECT (time: 3, fetch: 41, fetch_time: 0, ioread: 0)
              SCAN (temp time: 1, fetch: 19, ioread: 0, readrows: 8950, rows: 4950)
  
  CTE의 재귀절이 존재할 경우, 재귀절에 대한 힙 스캔은 병렬로 수행되지 않는다.
  
   .. code-block:: sql

    select col1, (select count(col2) from tbl t2 where t1.col1=t2.col2) c1 from tbl t1;

    Trace Statistics:
      SELECT (time: 3455, fetch: 212073, fetch_time: 25, ioread: 0)
        SCAN (table: dba.tbl), (heap time: 8, fetch: 4052, ioread: 0, readrows: 4000, rows: 4000)
        SUBQUERY (correlated)
          SELECT (time: 3439, fetch: 208000, fetch_time: 23, ioread: 0)
            SCAN (table: dba.tbl), (heap time: 3424, fetch: 208000, ioread: 0, readrows: 16000000, rows: 3200)
            SUBQUERY_CACHE (hit: 0, miss: 4000, size: 710584, status: enabled)
  
  상관 부질의가 존재할 경우, 해당 질의에 대한 힙 스캔은 병렬로 수행되지 않는다.

  3. CONNECT BY를 포함하는 질의의 경우:
   .. code-block:: sql

    select id, col1 from tbl where id <= 10
    start with id = 1 connect by prior id + 1 = id;

    Trace Statistics:
      SELECT (time: 4462, fetch: 309617, fetch_time: 38, ioread: 0)
        SCAN (table: dba.tbl), (heap time: 1, fetch: 52, ioread: 0, readrows: 4000, rows: 1)
        CONNECTBY (time: 4461, fetch: 309565, fetch_time: 0, ioread: 0)
          SCAN (table: dba.tbl), (heap time: 4004, fetch: 208000, ioread: 0, readrows: 16000000, rows: 3999)
          ORDERBY (time: 55, sort: true, page: 40, ioread: 0)

  connect by 절을 포함하는 질의의 경우, 해당 질의에 대한 힙 스캔은 병렬로 수행되지 않는다.

  4. incr(), decr() 함수 등을 클릭 카운터 함수를 포함한 질의의 경우:
   .. code-block:: sql

     select incr(id) from tbl where id = 1;

     Trace Statistics:
      SELECT (time: 0, fetch: 56, fetch_time: 0, ioread: 0)
        SCAN (table: dba.tbl), (heap time: 0, fetch: 52, ioread: 0, readrows: 4000, rows: 1)
  

  5. PL/CSQL, 자바 저장 프로시저가 WHERE 절 등 조건절에 포함된 경우 (SELECT 목록에 포함된 경우는 지원):
   .. code-block:: sql

     SELECT /*+ PARALLEL(4) */ * FROM employees 
     WHERE calculate_bonus(salary, performance_score) > 1000;

.. rubric:: 결과 병합 방식

병렬 처리의 결과 병합 방식은 크게 두 가지가 있다:

  1. **row by row** 방식
      - 작업자 스레드가 처리가 완료된 row를 즉시 질의 처리 스레드로 전송
      - 중앙 집중식 처리 방식으로, 작업자 스레드의 결과가 질의 처리 스레드에 직접 전달됨
  2. **mergeable list** 방식
      - 작업자 스레드가 결과를 임시 파일 형태로 저장
      - 질의 처리 스레드가 임시 파일들을 순차적으로 읽어서 결과를 이어붙임
      - 중앙 처리 집중형 방식이 아니어서 row by row 방식보다 성능상 이점이 있음
      - 대량의 데이터 처리에 더 적합한 방식

.. rubric:: 결과 병합 방식의 제약사항

mergeable list 방식은 row by row 방식보다 성능상 이점이 있지만, 다음과 같은 제약사항이 있다.
특히 row가 스캔될 때마다 추가 처리가 필요한 경우에는 이러한 제약사항이 발생한다.

다음 조건을 모두 만족하는 경우에만 mergeable list 방식이 적용된다:

  1. 최상위 질의이거나 비상관부질의일 것
  2. 병렬 힙 스캔 대상일 것
  3. 해시 집계를 수행하지 않을 것 (필요 시 `/*+ NO_HASH_AGGREGATE */` 힌트로 강제 가능)
  4. Top-N 정렬을 수행하지 않을 것
  5. 반환 값이 한 row로 정해진 집계 질의를 사용하지 않을 것 (COUNT(*) 등)
  6. SELECT 목록에 SET, MULTISET, SEQUENCE 컬렉션 타입이 포함되지 않을 것
  7. ROWNUM 또는 LIMIT 절을 사용하지 않을 것 
  8. 쿼리 캐시 기능이 비활성화 상태일 것
  9. PL/CSQL 또는 자바 저장 프로시저를 포함하지 않을 것
  10. MEDIAN, PERCENTILE_CONT, PERCENTILE_DIST, CUME_DIST, PERCENT_RANK 와 같은 분포 집계 함수를 사용하지 않을 것

대표적인 적용 예시는 다음과 같다:
 - 조인이 없는 단순 테이블 전체 스캔 쿼리
 - uncorrelated subquery
 - 테이블 전체 스캔 이후 ORDER BY, GROUP BY, 집계 함수 등의 후처리를 수행하는 쿼리

