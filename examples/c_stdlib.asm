GLOBAL c_put_char
GLOBAL c_print_int
GLOBAL c_rng_next
GLOBAL mod

EXTERN put_char
EXTERN print_int
EXTERN rng_next
EXTERN div

c_put_char:
  PEEKAR 0x04
  CALL put_char
  RET

c_print_int:
  PEEKAR 0x04
  CALL print_int
  RET

c_rng_next:
  PEEKAR 0x04
  CALL rng_next
  PUSHAR 0x06
  RET

mod:
  PEEKAR 0x04 A_B
  PEEKAR 0x06
  CALL div
  B_A PUSHAR 0x08
  RET
