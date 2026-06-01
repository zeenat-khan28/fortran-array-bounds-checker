! Test 6: Multi-dimensional array and named constant bounds
PROGRAM test6
  INTEGER, PARAMETER :: ROWS = 3, COLS = 4
  REAL :: GRID(ROWS, COLS)
  REAL :: CUBE(2, 3, 4)

  ! Named constant bounds — should be caught
  GRID(4, 1) = 1.0    ! ERROR: row 4 > ROWS(3)
  GRID(1, 5) = 2.0    ! ERROR: col 5 > COLS(4)
  GRID(0, 1) = 3.0    ! ERROR: row 0 < lower bound 1

  ! Correct usage
  GRID(1, 1) = 4.0    ! OK
  GRID(3, 4) = 5.0    ! OK

  ! 3D array
  CUBE(3, 1, 1) = 1.0  ! ERROR: dim1 index 3 > upper bound 2
  CUBE(1, 4, 1) = 2.0  ! ERROR: dim2 index 4 > upper bound 3
  CUBE(1, 1, 5) = 3.0  ! ERROR: dim3 index 5 > upper bound 4
  CUBE(2, 3, 4) = 4.0  ! OK
END PROGRAM
