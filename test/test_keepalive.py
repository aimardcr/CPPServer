import requests
import logging
from dataclasses import dataclass
from http import HTTPStatus
import time

# Basic logging
logging.basicConfig(format='%(message)s', level=logging.INFO)

@dataclass
class ServerConfig:
    url: str = "http://localhost:8000"

class TestServer:
    def __init__(self):
        self.config = ServerConfig()
        self.session = requests.Session()

    def test_keep_alive(self):
        """Test Keep-Alive functionality"""
        # Explicitly set Connection header to keep-alive
        headers = {'Connection': 'keep-alive'}
        
        # Make multiple requests and track connection details
        responses = []
        connection_ids = set()

        for i in range(5):
            # Send request with keep-alive header
            r = self.session.get(f"{self.config.url}/?name=TestKeepAlive{i}", 
                                 headers=headers)
            
            # Verify response
            assert r.status_code == 200
            assert r.text == f"Hello, TestKeepAlive{i}!"
            
            # Track response headers
            responses.append(r)
            
            # Check keep-alive headers
            assert 'Connection' in r.headers
            assert r.headers.get('Connection', '').lower() == 'keep-alive'
            assert 'Keep-Alive' in r.headers

    def test_keep_alive_performance(self):
        """Test Keep-Alive connection performance"""
        # Measure time for keep-alive vs non-keep-alive
        def measure_requests(use_keep_alive):
            session = requests.Session() if use_keep_alive else requests.Session()
            headers = {'Connection': 'keep-alive'} if use_keep_alive else {}
            
            start_time = time.time()
            for i in range(10):
                r = session.get(f"{self.config.url}/?name=PerformanceTest{i}", 
                                headers=headers)
                assert r.status_code == 200
            end_time = time.time()
            
            return end_time - start_time

        # Compare performance
        keep_alive_time = measure_requests(True)
        regular_time = measure_requests(False)
        
        logging.info(f"Keep-Alive Time: {keep_alive_time:.4f} seconds")
        logging.info(f"Regular Time: {regular_time:.4f} seconds")
        
        # Keep-alive should be faster (at least 20% improvement)
        assert keep_alive_time < regular_time * 0.8, "Keep-Alive did not improve performance"

    def test_keep_alive_timeout(self):
        """Test Keep-Alive connection timeout"""
        headers = {'Connection': 'keep-alive'}
        
        # First request
        r1 = self.session.get(f"{self.config.url}/?name=TimeoutTest1", 
                              headers=headers)
        assert r1.status_code == 200
        
        # Wait longer than the keep-alive timeout
        time.sleep(6)  # Assuming 5-second timeout in server config
        
        # Subsequent request should still work
        r2 = self.session.get(f"{self.config.url}/?name=TimeoutTest2", 
                              headers=headers)
        assert r2.status_code == 200
        assert r2.text == "Hello, TimeoutTest2!"

def run_tests():
    """Run all tests"""
    server = TestServer()
    tests = [
        server.test_keep_alive,
        server.test_keep_alive_performance,
        server.test_keep_alive_timeout,
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