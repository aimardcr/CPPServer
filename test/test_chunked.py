import socket
import json
import time
import logging

logging.basicConfig(format='%(message)s', level=logging.INFO)

class ChunkedTest:
    def __init__(self, host='localhost', port=8000):
        self.addr = (host, port)

    def send_chunked(self, path, chunks, content_type='text/plain'):
        """Send data in chunks and return response"""
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect(self.addr)
            
            # Send headers
            headers = (
                f"POST {path} HTTP/1.1\r\n"
                f"Host: {self.addr[0]}:{self.addr[1]}\r\n"
                f"Content-Type: {content_type}\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
            )
            sock.sendall(headers.encode())
            
            # Send chunks
            for chunk in chunks:
                if isinstance(chunk, str):
                    chunk = chunk.encode()
                    
                # Send chunk size and data
                sock.sendall(f"{len(chunk):x}\r\n".encode())
                sock.sendall(chunk)
                sock.sendall(b"\r\n")
                time.sleep(0.1)  # Small delay between chunks
            
            # End chunked transfer
            sock.sendall(b"0\r\n\r\n")
            
            # Get response
            return sock.recv(4096).decode()

def run_tests():
    """Run chunked transfer tests"""
    client = ChunkedTest()
    
    # Test cases: (name, content_type, chunks)
    tests = [
        (
            "Text Streaming",
            "text/plain",
            ["First chunk\n", "Second chunk\n", "Last chunk"]
        ),
        (
            "JSON Streaming",
            "application/json",
            [
                '{"name": "test",',
                '"data": [1,2,3],',
                '"status": "ok"}'
            ]
        ),
        (
            "Binary Streaming",
            "application/octet-stream",
            [bytes([i % 256]) for i in range(10)]
        )
    ]

    for name, content_type, chunks in tests:
        try:
            logging.info(f"\nTesting {name}...")
            response = client.send_chunked("/test-chunked", chunks, content_type)
            assert "200 OK" in response and "Successfully received" in response
            logging.info(f"✓ {name} passed")
        except AssertionError:
            logging.error(f"✗ {name} failed - Unexpected response: {response}")
        except Exception as e:
            logging.error(f"✗ {name} failed - Error: {str(e)}")

if __name__ == "__main__":
    run_tests()