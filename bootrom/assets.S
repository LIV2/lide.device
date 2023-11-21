  section BOOTLDR
    IFND WORDWIDE
  incbin "obj/bootnibbles"
    ELSE
  incbin "obj/bootldr-word"
    ENDIF
  
  section DEVICE
  incbin "../lide.device"
