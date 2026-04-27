#!/usr/bin/env python3
"""
Test script for Lucid Control Server API
Tests the search functionality endpoints
"""

import http.client
import json
import sys

PORT = 8765

def make_request(method, path, body=None):
    """Make HTTP request to control server"""
    conn = http.client.HTTPConnection("127.0.0.1", PORT, timeout=10)
    try:
        headers = {"Content-Type": "application/json"}
        conn.request(method, path, body=body, headers=headers)
        resp = conn.getresponse()
        data = resp.read().decode()
        return resp.status, json.loads(data) if data else {}
    finally:
        conn.close()

def test_health():
    """Test health endpoint"""
    print("=== Testing /health ===")
    status, data = make_request("GET", "/health")
    print(f"Status: {status}")
    print(f"Response: {json.dumps(data, indent=2)}")
    assert status == 200, f"Expected 200, got {status}"
    assert data.get("status") == "ok", f"Expected ok status"
    print("✅ Health check passed\n")

def test_semantic_search(query="machine learning", max_results=5):
    """Test semantic search endpoint"""
    print(f"=== Testing /api/command semantic_search (query='{query}') ===")
    body = json.dumps({
        "command": "semantic_search",
        "args": {
            "query": query,
            "max_results": max_results
        }
    })
    status, data = make_request("POST", "/api/command", body)
    print(f"Status: {status}")
    print(f"Response: {json.dumps(data, indent=2)[:500]}...")
    
    if "error" in data and "not initialized" in data.get("error", ""):
        print("⚠️  Semantic search not initialized (no embedding model)")
        return None
    
    assert status == 200, f"Expected 200, got {status}"
    assert data.get("success") == True, f"Expected success"
    print(f"✅ Found {data.get('result_count', 0)} results\n")
    return data.get("results", [])

def test_hybrid_search(query="python tutorial", max_results=5):
    """Test hybrid search endpoint"""
    print(f"=== Testing /api/command hybrid_search (query='{query}') ===")
    body = json.dumps({
        "command": "hybrid_search",
        "args": {
            "query": query,
            "max_results": max_results,
            "semantic_weight": 0.7,
            "keyword_weight": 0.3
        }
    })
    status, data = make_request("POST", "/api/command", body)
    print(f"Status: {status}")
    print(f"Response: {json.dumps(data, indent=2)[:500]}...")
    
    if "error" in data and "not initialized" in data.get("error", ""):
        print("⚠️  Hybrid search not initialized")
        return None
    
    assert status == 200, f"Expected 200, got {status}"
    print(f"✅ Found {data.get('result_count', 0)} results\n")
    return data.get("results", [])

def test_search_in_video(video_id, query="test"):
    """Test search within specific video"""
    print(f"=== Testing /api/command search_in_video (video_id='{video_id}', query='{query}') ===")
    body = json.dumps({
        "command": "search_in_video",
        "args": {
            "video_id": video_id,
            "query": query,
            "max_results": 5
        }
    })
    status, data = make_request("POST", "/api/command", body)
    print(f"Status: {status}")
    print(f"Response: {json.dumps(data, indent=2)[:500]}...")
    
    if "error" in data and "not initialized" in data.get("error", ""):
        print("⚠️  Search in video not initialized")
        return None
    
    assert status == 200, f"Expected 200, got {status}"
    print(f"✅ Found {data.get('result_count', 0)} results\n")
    return data.get("results", [])

def test_get_search_stats():
    """Test get search stats endpoint"""
    print("=== Testing /api/command get_search_stats ===")
    body = json.dumps({
        "command": "get_search_stats",
        "args": {}
    })
    status, data = make_request("POST", "/api/command", body)
    print(f"Status: {status}")
    print(f"Response: {json.dumps(data, indent=2)}")
    
    if "error" in data and "not initialized" in data.get("error", ""):
        print("⚠️  Search stats not available")
        return None
    
    assert status == 200, f"Expected 200, got {status}"
    print("✅ Search stats retrieved\n")
    return data

def test_get_videos(limit=5):
    """Test get videos endpoint"""
    print(f"=== Testing /api/command get_videos (limit={limit}) ===")
    body = json.dumps({
        "command": "get_videos",
        "args": {
            "limit": limit
        }
    })
    status, data = make_request("POST", "/api/command", body)
    print(f"Status: {status}")
    print(f"Total videos: {data.get('total', 0)}")
    
    videos = data.get("videos", [])
    print(f"Got {len(videos)} videos")
    if videos:
        print(f"First video: {videos[0].get('title', 'unknown')[:50]}")
    print()
    return videos

def main():
    print("=" * 60)
    print("Lucid Control Server API Test")
    print("=" * 60 + "\n")
    
    try:
        # Test basic connectivity
        test_health()
        
        # Test search endpoints
        test_semantic_search("machine learning")
        test_hybrid_search("python tutorial")
        test_get_search_stats()
        
        # Get videos first to find a video_id for search_in_video test
        videos = test_get_videos(5)
        
        if videos:
            # Test search within a specific video
            video_id = videos[0].get("id")
            if video_id:
                test_search_in_video(video_id, "test")
        
        print("=" * 60)
        print("All tests completed!")
        print("=" * 60)
        return 0
        
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())