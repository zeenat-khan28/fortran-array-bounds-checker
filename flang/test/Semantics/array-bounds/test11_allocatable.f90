! Test 11: Allocatable array bounds tracking
PROGRAM test11
  REAL, ALLOCATABLE :: A(:)
  REAL, ALLOCATABLE :: B(:,:)
  INTEGER :: N

  N = 10
  ALLOCATE(A(10))      ! A now has bounds [1:10]
  ALLOCATE(B(3,4))     ! B now has bounds [1:3][1:4]

  A(5)  = 1.0   ! OK: 5 within [1:10]
  A(10) = 2.0   ! OK: 10 is upper bound
  A(11) = 3.0   ! ERROR: 11 > upper bound 10
  A(0)  = 4.0   ! ERROR: 0 < lower bound 1

  B(2,3) = 5.0  ! OK: within bounds
  B(4,1) = 6.0  ! ERROR: row 4 > upper bound 3
  B(1,5) = 7.0  ! ERROR: col 5 > upper bound 4

  DEALLOCATE(A)
  DEALLOCATE(B)
END PROGRAM
