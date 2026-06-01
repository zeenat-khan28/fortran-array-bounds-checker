! Test 8: Array slice bounds checking
PROGRAM test8
  REAL :: A(1:10)
  REAL :: GRID(3, 4)
  REAL :: B(5)

  ! Scalar slice checks
  PRINT*, A(2:8)     ! OK: slice [2:8] within [1:10]
  PRINT*, A(1:10)    ! OK: full range
  PRINT*, A(0:5)     ! ERROR: slice lower 0 < bound 1
  PRINT*, A(5:15)    ! ERROR: slice upper 15 > bound 10
  PRINT*, A(-1:12)   ! ERROR: both bounds wrong

  ! 2D array slices
  PRINT*, GRID(1:4, 1)   ! ERROR: row slice upper 4 > bound 3
  PRINT*, GRID(1:3, 1:4) ! OK: full range

  ! Stride (we only check lower/upper, not stride)
  PRINT*, A(1:10:2)  ! OK: stride 2 but bounds fine
  PRINT*, A(1:11:2)  ! ERROR: upper 11 > bound 10
END PROGRAM
