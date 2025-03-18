.. _parallel-execution:

병렬 처리
========================================

CUBRID는 대용량 데이터를 효율적으로 처리하기 위해 힙 스캔 작업을 여러 작업자(worker)가 동시에 수행하는 병렬 처리를 지원한다.

각 작업자는 테이블의 서로 다른 페이지를 독립적으로 처리한 후, 그 결과를 통합하는 방식으로 동작한다.

이를 통해 전체 질의 실행 시간을 단축할 수 있다.

.. rubric:: SQL 힌트

병렬 처리를 사용하려면 다음과 같은 SQL 힌트를 사용해야 한다.

  .. code-block:: sql

    /*+ PARALLEL(N) */

여기서 N은 병렬 처리에 사용할 작업자의 수를 지정한다.
예를 들어, **PARALLEL(4)** 힌트는 4개의 작업자를 사용하여 질의를 병렬 처리한다.

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
  2. CTE의 재귀 부분(recursive_part)이나 상관 관계가 있는 서브쿼리(correlated subquery)에 포함되는 경우
  3. CONNECT BY를 포함하는 질의의 경우
  4. incr(), decr() 함수 등을 클릭 카운터 함수를 포함한 질의의 경우
  5. 저장 프로시저, 저장함수를 포함하는 질의의 경우
  6. SELECT를 제외한 질의(UNION, INTERSECTION, DIFFERENCE, INSERT, UPDATE, DELETE 등)
  7. JOIN 구문에서 첫 번째 테이블이 아닌 경우
  8. 시스템 카탈로그나 시스템 클래스에 접근하는 경우
  9. SELECT FOR UPDATE 구문(SELECT 잠금 필요)

.. rubric:: 성능 관점에서 권장되는 경우

기술적으로 지원되는 경우라도, 다음 조건을 모두 만족할 때 병렬 힙 스캔을 사용하면 성능상 이점이 있다.

  1. 테이블의 크기가 충분히 큰 경우
  2. 시스템의 CPU 코어 수가 충분한 경우
  3. 충분한 메모리가 사용 가능한 경우
  4. WHERE 절의 조건으로 많은 데이터를 걸러낼 수 있는 경우

.. rubric:: 성능 관점에서 권장되지 않는 경우

다음과 같은 경우에는 병렬 힙 스캔이 기술적으로 가능하더라도 성능 저하를 초래할 수 있어 사용을 권장하지 않는다.

  1. 테이블의 대부분의 레코드를 읽어야 하는 경우(결과 병합 비용이 높음)
  2. 시스템의 가용 메모리가 제한적인 경우
  3. WHERE 절의 조건으로 걸러지는 데이터가 적은 경우

.. note::

  병렬 처리는 시스템 리소스를 많이 사용하므로, 적절한 상황에서만 사용하는 것이 좋다.

  각 작업자가 처리한 데이터를 임시 볼륨과 메모리에 저장하기 때문에, 단일 처리 대비 더 많은 메모리를 사용한다.

  특히 WHERE 절의 조건으로 걸러지는 데이터가 적은 경우, 여러 작업자들이 읽은 많은 결과를 병합하는 과정에서 큰 성능 저하가 발생할 수 있다.
  
  따라서, 질의의 카디널리티(cardinality)가 낮을 수록 병렬 처리 성능은 향상된다.

.. rubric:: 작업자 수 결정

.. rubric:: 병렬 힙 스캔 비활성화 방법

다음과 같은 방법으로 병렬 힙 스캔을 명시적으로 비활성화할 수 있다.

  1. SQL 힌트 사용:

     .. code-block:: sql

       /*+ NO_PARALLEL_HEAP_SCAN */

  2. 서버 파라미터 설정:

     .. code-block:: sql

       SET SYSTEM PARAMETERS 'parallel_heap_scan_threads=0';

.. _example_parallel-execution_heap-scan:

.. rubric:: 예시 및 프로파일링 결과

다음은 병렬 처리를 사용하여 힙 스캔을 수행하는 예시이다:

  .. code-block:: sql

    /* 4개의 작업자를 사용하여 병렬로 힙 스캔 수행 */
    SELECT /*+ PARALLEL(4) */ count(*) 
    FROM tmp 
    WHERE col1 like '%1010%' 
    AND col2 like '%10101%';

질의 프로파일링 결과에서도 병렬 처리가 어떻게 이루어졌는지 확인할 수 있다:

  .. code-block:: none

    Query Plan:
      TABLE SCAN (dba.tmp)
    
      rewritten query: select /*+ PARALLEL(4) */ count(*) 
      from [dba.tmp] [dba.tmp] 
      where [dba.tmp].col1 like ?:0 and [dba.tmp].col2 like ?:1 

    Trace Statistics:
      SELECT (time: 6245, fetch: 493896, fetch_time: 2150, ioread: 123465)
        SCAN (table: dba.tmp), (heap time: 6235, fetch: 493896, ioread: 123465, readrows: 299, rows: 299)
             (table: dba.tmp), (parallel heap time: 6233, readrows: 2485875, rows: 83)
             (table: dba.tmp), (parallel heap time: 6233, readrows: 2501523, rows: 84)
             (table: dba.tmp), (parallel heap time: 6233, readrows: 2505411, rows: 82)
             (table: dba.tmp), (parallel heap time: 6233, readrows: 2507191, rows: 50)

프로파일링 결과를 통해 다음과 같은 정보를 확인할 수 있다:

  1. 각 작업자의 처리 결과:
      - 첫 번째 작업자: 2,485,875개의 레코드를 읽어서 83개의 결과를 반환
      - 두 번째 작업자: 2,501,523개의 레코드를 읽어서 84개의 결과를 반환
      - 세 번째 작업자: 2,505,411개의 레코드를 읽어서 82개의 결과를 반환
      - 네 번째 작업자: 2,507,191개의 레코드를 읽어서 50개의 결과를 반환
  2. 전체 처리 결과:
      - 총 실행 시간: 6,245ms
      - 총 fetch 횟수: 493,896회
      - 총 I/O 읽기 횟수: 123,465회
      - 최종 결과 레코드 수: 299개 (각 작업자의 결과를 병합)

이 예시에서는 약 1천만 개의 레코드(각 작업자가 약 250만 개씩 처리)를 읽어서 WHERE 절의 조건으로 299개의 레코드만 선택되었다.

이는 WHERE 절의 조건으로 많은 데이터를 걸러낼 수 있는 경우로, 병렬 힙 스캔이 효과적으로 동작하는 이상적인 상황을 보여준다.
