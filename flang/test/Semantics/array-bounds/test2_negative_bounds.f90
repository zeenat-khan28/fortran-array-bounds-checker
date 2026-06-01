! Test 2: Array with negative lower bound
PROGRAM test2
  REAL :: B(-5:5)
  B(-6) = 1.0
  B(-5) = 2.0
  B(5)  = 3.0
  B(6)  = 4.0
END PROGRAM
