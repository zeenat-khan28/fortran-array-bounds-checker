! Test 10: Function argument bounds checking
SUBROUTINE fill10(A)
  REAL :: A(10)     ! expects array of size 10
  A(1) = 0.0
END SUBROUTINE

SUBROUTINE fill5(A)
  REAL :: A(5)      ! expects array of size 5
  A(1) = 0.0
END SUBROUTINE

PROGRAM test10
  REAL :: BIG(20)   ! size 20
  REAL :: SMALL(3)  ! size 3

  CALL fill10(BIG)    ! OK: 20 >= 10
  CALL fill10(SMALL)  ! WARNING: 3 < 10
  CALL fill5(BIG)     ! OK: 20 >= 5
  CALL fill5(SMALL)   ! WARNING: 3 < 5
END PROGRAM
