! Test 3: Variable index — checker emits advisory warning
PROGRAM test3
  REAL    :: C(1:10)
  INTEGER :: idx
  READ(*,*) idx
  C(idx) = 99.0
END PROGRAM
