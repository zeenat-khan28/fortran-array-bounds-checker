! Test 5: All accesses within bounds — expect zero diagnostics
PROGRAM test5
  REAL    :: E(0:9)
  INTEGER :: j
  E(0) = 1.0
  E(9) = 2.0
  E(5) = 3.0
  DO j = 0, 9
    E(j) = REAL(j)
  END DO
END PROGRAM
