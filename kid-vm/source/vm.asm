    [BITS 16]
    mov sp,0x3000
    call main
    hlt

main:
    mov ax,hellostring
    mov bx,hellolen
    call write
main_loop:
    mov ax,liststring
    mov bx,listlen
    call write
    mov ax,choice
    mov bx,1
    call read
    mov si,choice
    mov al,[si]
    cmp al,0x31
    jz case1
    cmp al,0x32
    jz case2
    cmp al,0x33
    jz case3
    cmp al,0x34
    jz case4
    cmp al,0x35
    jz case5
    cmp al,0x36
    jz case6
    cmp al,0x37
    jz case7
    mov ax,wrong
    mov bx,wronglen
    call write
    jmp next_loop

case1:
    call alloc
    jmp next_loop

case2:
    call update
    jmp next_loop

case3:
    call free
    jmp next_loop

case4:
    call alloc_host
    jmp next_loop

case5:
    call update_host
    jmp next_loop

case6:
    call free_host
    jmp next_loop

case7:
    ret

next_loop:
    jmp main_loop

alloc:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov ax,sizestring
    mov bx,sizestringlen
    call write
    mov ax,size
    mov bx,2
    call read
    mov ax,[size]
    cmp ax,0x1000
    ja size_out
    mov cx,[total_size]
    cmp cx,0xb000
    ja total_size_out
    mov si,[num]
    cmp si,0x10
    jae num_out
    mov di,cx
    add cx,0x5000
    add si,si
    mov [list+si],cx
    mov [list_size+si],ax
    add di,ax
    mov [total_size],di
    mov al,[num]
    inc al
    mov [num],al
    jmp alloc_out
size_out:
    mov ax,sizetoomanystring
    mov bx,sizetoomanystringlen
    call write
    jmp alloc_out
total_size_out:
    mov ax,guestfullstring
    mov bx,guestfullstringlen
    call write
    jmp alloc_out
num_out:
    mov ax,numtoomanystring
    mov bx,numtoomanystringlen
    call write
alloc_out:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

update:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov ax,idxstring
    mov bx,idxstringlen
    call write
    mov ax,idx
    mov bx,1
    call read
    mov ax,contentstring
    mov bx,contentstringlen
    call write
    mov al,[idx]
    cmp al,[num]
    jae idx_out
    movzx si,al
    add si,si
    mov ax,[list+si]
    mov bx,[list_size+si]
    call read
idx_out:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

free:
    ret

alloc_host:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov ax,sizestring
    mov bx,sizestringlen
    call write
    mov ax,size
    mov bx,2
    call read
    push 100h
    popf
    mov bx,[size]
    mov ax,100h
    vmcall
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

update_host:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov ax,sizestring
    mov bx,sizestringlen
    call write
    mov ax,size
    mov bx,2
    call read
    mov ax,[size]
    cmp ax,0x1000
    ja update_host_out
    mov ax,idxstring
    mov bx,idxstringlen
    call write
    mov ax,idx
    mov bx,1
    call read 
    mov ax,contentstring
    mov bx,contentstringlen
    call write
    mov ax,0x4000
    mov bx,[size]
    call read
    push 100h
    popf
    mov ax,102h
    mov bx,1
    mov cl,[idx]
    mov dx,[size]
    vmcall
    jmp update_host_out
update_host_size_out:
    mov ax,sizetoomanystring
    mov bx,sizetoomanystringlen
    call write
update_host_out:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

free_host:
    push ax
    push bx
    push cx
    push dx
    push si
    push di    
    mov ax,idxstring
    mov bx,idxstringlen
    call write
    mov ax,idx
    mov bx,1
    call read
    push 100h
    popf
    mov ax,101h
    mov bx,3
    mov cl,[idx]
    vmcall
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret


write:
    push cx
    push dx
    push si
    mov cx,bx
    mov si,ax
write_loop:
    mov al,[si]
    out 17h,al
    inc si
    loop write_loop
    pop si
    pop dx
    pop cx
    ret
read:
    push cx
    push dx
    push si
    mov cx,bx
    mov si,ax
read_loop:
    in al,17h
    mov [si],al
    inc si
    loop read_loop
    pop si
    pop dx
    pop cx
    ret
    ;; data
    hellostring db 'Welcome to the Virtual World Memory Management!', 0x0a
    hellolen equ $-hellostring
    liststring db 'What do you want to do?', 0x0a, '1.Alloc memory',0x0a,'2.Update memory',0x0a,'3.Free memory',0x0a,'4.Alloc host memory',0x0a,'5.Update host memory',0x0a,'6.Free host memory',0x0a,'7.Exit',0x0a,'Your choice:'
    listlen equ $-liststring
    wrong db 'Wrong',0x0a
    wronglen equ $-wrong
    idxstring db 'Index:'
    idxstringlen equ $-idxstring
    sizestring db 'Size:'
    sizestringlen equ $-sizestring
    sizetoomanystring db 'Too big',0x0a
    sizetoomanystringlen equ $-sizetoomanystring
    guestfullstring db 'Guest memory is full! Please use the host memory!',0x0a
    guestfullstringlen equ $-guestfullstring
    numtoomanystring db 'Too many memory',0x0a
    numtoomanystringlen equ $-numtoomanystring
    contentstring db 'Content:'
    contentstringlen equ $-contentstring
    choice dw 0
    size dw 0
    num db 0
    idx db 0
    total_size dw 0
    list resw 10h
    list_size resw 10h