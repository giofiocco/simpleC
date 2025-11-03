GLOBAL c_put_char
GLOBAL c_print_int

EXTERN put_char
EXTERN print_int

c_put_char:
  PEEKAR 0x04
  CALL put_char
  RET

c_print_int:
  PEEKAR 0x04
  CALL print_int
  RET
