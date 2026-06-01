! Test 7: Assumed-size array detection
SUBROUTINE process(A, B, N)
  INTEGER, INTENT(IN) :: N
  REAL :: A(*)          ! assumed-size — upper bound unknown
  REAL :: B(1:N)        ! explicit upper bound using variable

  A(1)   = 1.0          ! WARNING: assumed-size, cannot verify
  A(100) = 2.0          ! WARNING: assumed-size, cannot verify
  B(1)   = 3.0          ! WARNING: B upper bound N is not constant
END SUBROUTINE

PROGRAM test7
  REAL :: X(10), Y(20)
  CALL process(X, Y, 10)
END PROGRAM
