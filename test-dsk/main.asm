	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; header
	
	output "main.bin"
	org $8000-128
	include cpc.inc
	
	db $00 ;user number
	db "MAIN    " ;filename
	db "BIN" ;extension
	ds 4,0 ;unused
	db 0,0 ;block number/last block (unused)
	db $02 ;file type
	dw end-load ;data length
	dw load ;data location
	db 0 ;first block (unused?)
	dw end-load ;logical length
	dw entry ;entry address
	ds 64-28,0 ;unused
	
	d24 end-load ;file length
	dw 0 ;checksum
	ds 128-69,0
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; defines
	
	
OVERSCAN_H_TOTAL equ 64
OVERSCAN_H_DISPLAYED equ 50
OVERSCAN_HSYNC equ 50
OVERSCAN_HSYNC_WIDTH equ 14
OVERSCAN_V_TOTAL equ 39
OVERSCAN_V_ADJUST equ 0
OVERSCAN_V_DISPLAYED equ 38
OVERSCAN_VSYNC equ 38
OVERSCAN_VSYNC_HEIGHT equ 8
	
OVERSCAN_BITMAP equ $0050
OVERSCAN_IMAGE_WIDTH equ OVERSCAN_H_DISPLAYED * 8 / 2
OVERSCAN_IMAGE_HEIGHT equ OVERSCAN_V_DISPLAYED * 8
	
	
	
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; main
	
load:
entry:
	
	ld de,end
	call KL_ROM_WALK
	
	
	;;;;;;;;;; init video
	
	xor a
	call SCR_SET_MODE
	
	di
	
	ld hl,OVERSCAN_BITMAP
	ld de,OVERSCAN_BITMAP+1
	ld bc,((OVERSCAN_BITMAP & $c000) + $8000) - (OVERSCAN_BITMAP & $3ff) - 1
	ld (hl),0
	ldir
	
	ld hl,overscan_crtc_regs
	xor a
1
	ld b,$bc
	out (c),a
	
	inc b
	ld c,(hl)
	out (c),c
	inc hl
	
	inc a
	cp overscan_crtc_regs_end-overscan_crtc_regs
	jr c,1b
	
	ei
	
	
	;;;;;;;;;;; read from disk
	
	ld hl,image_filename
	ld b,image_filename_end-image_filename
	ld de,cas_buffer
	call CAS_IN_OPEN
	
	ld hl,OVERSCAN_BITMAP-$10
	call CAS_IN_DIRECT
	
	call CAS_IN_CLOSE
	
	ld hl,OVERSCAN_BITMAP-$10
	ld a,0
1	push af
	push hl
	ld b,(hl)
	ld c,b
	call SCR_SET_INK
	pop hl
	pop af
	inc l
	inc a
	cp $10
	jr c,1b
	
	jr $
	

image_filename:
	.db "IMAGE.BIN"
image_filename_end:
	
	
	
overscan_crtc_regs:
	db OVERSCAN_H_TOTAL-1
	db OVERSCAN_H_DISPLAYED
	db OVERSCAN_HSYNC
	db (OVERSCAN_VSYNC_HEIGHT << 4) | OVERSCAN_HSYNC_WIDTH
	db OVERSCAN_V_TOTAL-1
	db OVERSCAN_V_ADJUST
	db OVERSCAN_V_DISPLAYED
	db OVERSCAN_VSYNC
	db 0
	db 7
	db 0,0
	db ((OVERSCAN_BITMAP & $c000) >> 10) | $0c | ((OVERSCAN_BITMAP & $0600) >> 9)
	db (OVERSCAN_BITMAP & $1fe) >> 1
overscan_crtc_regs_end:
	
	
	
	
	
end:

cas_buffer equ $c000
	
	