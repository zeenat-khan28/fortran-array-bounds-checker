! Test 4: DO loop variable exceeds array bounds at upper end
PROGRAM test4
  REAL    :: D(1:10)
  INTEGER :: i
  DO i = 1, 12
    D(i) = REAL(i)
  END DO
  DO i = 1, 10
    D(i) = REAL(i) * 2
  END DO
END PROGRAM
