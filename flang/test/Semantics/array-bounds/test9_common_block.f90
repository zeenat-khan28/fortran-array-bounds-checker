! Test 9: Common block array tracking
PROGRAM test9
  REAL :: A(10), B(5), C(20)
  COMMON /myblock/ A, B      ! A and B share common block /myblock/
  COMMON /otherblock/ C      ! C in separate block

  ! Access through common block arrays
  A(5)  = 1.0   ! OK: 5 within [1:10]
  A(11) = 2.0   ! ERROR: 11 > upper bound 10
  B(3)  = 3.0   ! OK: 3 within [1:5]
  B(6)  = 4.0   ! ERROR: 6 > upper bound 5
  C(20) = 5.0   ! OK: 20 within [1:20]
  C(21) = 6.0   ! ERROR: 21 > upper bound 20
END PROGRAM
