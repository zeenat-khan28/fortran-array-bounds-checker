! Test 1: Constant index clearly out of bounds — expect compile error
PROGRAM test1
  REAL :: A(1:10)
  A(0)  = 1.0
  A(11) = 2.0
  A(5)  = 3.0
END PROGRAM
