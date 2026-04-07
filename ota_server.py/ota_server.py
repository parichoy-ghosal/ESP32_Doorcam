import socket

HOST = "0.0.0.0"
PORT = 8000

def serve_client(conn):
    request = conn.recv(1024).decode()
    print("Request:\n", request)

    if "GET /version.txt" in request:
        with open("version.txt", "r") as f:
            version = f.read().strip()

        response_body = version + "\n"

        response = (
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            f"Content-Length: {len(response_body)}\r\n"
            "Connection: close\r\n"
            "\r\n"
            f"{response_body}"
        )

        conn.sendall(response.encode())
        print("Sent version")

    elif "GET /firmware.bin" in request:
        with open("firmware.bin", "rb") as f:
            data = f.read()

        headers = (
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/octet-stream\r\n"
            f"Content-Length: {len(data)}\r\n"
            "Connection: close\r\n"
            "\r\n"
        )

        conn.sendall(headers.encode())
        conn.sendall(data)

        print(f"Sent firmware ({len(data)} bytes)")

    else:
        response = (
            "HTTP/1.1 404 Not Found\r\n"
            "Connection: close\r\n"
            "\r\n"
        )
        conn.sendall(response.encode())

    conn.close()


# -------- MAIN LOOP --------
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.bind((HOST, PORT))
server.listen(5)

print(f"RAW OTA Server running on port {PORT}")

while True:
    conn, addr = server.accept()
    print(f"Connection from {addr}")
    serve_client(conn)
