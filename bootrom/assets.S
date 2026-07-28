  section BOOTLDR
    IFND WORDWIDE
  incbin "obj/bootnibbles"
    ELSE
  incbin "obj/bootldr-word"
    ENDIF
  include stage2.S
  section DEVICE
  incbin "../build/lide.device"
