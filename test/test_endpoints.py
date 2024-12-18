import requests
import logging
from dataclasses import dataclass
from http import HTTPStatus

# Basic logging
logging.basicConfig(format='%(message)s', level=logging.INFO)

@dataclass
class ServerConfig:
    url: str = "http://localhost:8000"

class TestServer:
    def __init__(self):
        self.config = ServerConfig()
        self.session = requests.Session()

    def test_root(self):
        """Test GET / endpoint"""
        # With name
        r = self.session.get(f"{self.config.url}/?name=John")
        assert r.status_code == 200 and r.text == "Hello, John!"

        # Empty name
        r = self.session.get(f"{self.config.url}/?name=")
        assert r.status_code == 400 and r.text == "Name is required"

        # Long name
        r = self.session.get(f"{self.config.url}/?name={'a' * 101}")
        assert r.status_code == 400 and r.text == "Name is too long"

        # No name
        r = self.session.get(f"{self.config.url}/")
        assert r.status_code == 200 and r.text == "Hello, world!"

    def test_contact(self):
        """Test GET /get-contact endpoint"""
        r = self.session.get(f"{self.config.url}/get-contact")
        assert r.status_code == 200
        data = r.json()
        assert data["name"] == "John Doe"
        assert data["email"] == "john.doe@mail.com"

    def test_cookie(self):
        """Test GET /set-cookie endpoint"""
        r = self.session.get(f"{self.config.url}/set-cookie")
        assert r.status_code == 200
        assert "Set-Cookie" in r.headers
        assert "name=value" in r.headers["Set-Cookie"]

    def test_submit(self):
        """Test /submit-data endpoint"""
        data = {"name": "John", "email": "john.doe@mail.com"}
        expected = "Name: John, Email: john.doe@mail.com"

        # JSON
        r = self.session.post(f"{self.config.url}/submit-data", json=data)
        assert r.status_code == 200 and r.text == expected

        # Form
        r = self.session.post(f"{self.config.url}/submit-data", data=data)
        assert r.status_code == 200 and r.text == expected

        # Invalid JSON
        r = self.session.post(f"{self.config.url}/submit-data", 
                            json={"email": "test@example.com"})
        assert r.status_code == 400 and r.text == "Name is required"

        # Invalid form
        r = self.session.post(f"{self.config.url}/submit-data", 
                            data={"name": "John"})
        assert r.status_code == 400 and r.text == "Email is required"

    def test_upload(self):
        """Test POST /upload-file endpoint"""
        # Valid upload
        files = {'file': ('test.txt', 'This is a test file')}
        r = self.session.post(f"{self.config.url}/upload-file", files=files)
        assert r.status_code == 200 and r.text == "File uploaded successfully"

        # No file
        r = self.session.post(f"{self.config.url}/upload-file")
        assert r.status_code == 400 and r.text == "No files uploaded"

def run_tests():
    """Run all tests"""
    server = TestServer()
    tests = [
        server.test_root,
        server.test_contact,
        server.test_cookie,
        server.test_submit,
        server.test_upload
    ]

    passed = failed = 0
    for test in tests:
        try:
            test()
            logging.info(f"✓ {test.__doc__}")
            passed += 1
        except AssertionError as e:
            logging.error(f"✗ {test.__doc__}: {str(e)}")
            failed += 1
        except Exception as e:
            logging.error(f"✗ {test.__doc__}: Unexpected error: {str(e)}")
            failed += 1

    total = passed + failed
    logging.info(f"\nResults: {passed}/{total} tests passed")
    return failed == 0

if __name__ == "__main__":
    import sys
    sys.exit(0 if run_tests() else 1)