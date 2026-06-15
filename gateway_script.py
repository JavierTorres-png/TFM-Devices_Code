import socket

socket = socket.socket()
socket.bind(("0.0.0.0", 8888))
socket.listen(10) #Allow up to 10 unaceppted connections, then reject the new ones

print("Server listening in port 8888")

conn, addr = socket.accept()
print("Accepted connection from: ", addr)

while True:
	data = conn.recv(1024)
	if not(data):
		break
	print("Received:", data.decode())

conn.close()
