#!/bin/bash
# Fe64 Chess Engine - Quick Test Script
# Tests all major features of the engine

ENGINE="../bin/bbc"
TESTS_PASSED=0
TESTS_FAILED=0

echo "=================================================="
echo "  Fe64 Chess Engine - Feature Test Suite"
echo "=================================================="
echo ""

# Test 1: UCI Protocol
echo -n "Test 1: UCI Protocol... "
RESULT=$(echo "uci" | timeout 5 $ENGINE 2>&1 | grep -c "uciok")
if [ "$RESULT" -ge 1 ]; then
    echo "PASSED ✓"
    ((TESTS_PASSED++))
else
    echo "FAILED ✗"
    ((TESTS_FAILED++))
fi

# Test 2: Ready Check
echo -n "Test 2: Ready Check... "
RESULT=$(echo -e "uci\nisready" | timeout 5 $ENGINE 2>&1 | grep -c "readyok")
if [ "$RESULT" -ge 1 ]; then
    echo "PASSED ✓"
    ((TESTS_PASSED++))
else
    echo "FAILED ✗"
    ((TESTS_FAILED++))
fi

# Test 3: Basic Search
echo -n "Test 3: Basic Search... "
RESULT=$(echo -e "uci\nisready\nposition startpos\ngo depth 5\nquit" | timeout 30 $ENGINE 2>&1 | grep -c "bestmove")
if [ "$RESULT" -ge 1 ]; then
    echo "PASSED ✓"
    ((TESTS_PASSED++))
else
    echo "FAILED ✗"
    ((TESTS_FAILED++))
fi

# Test 4: Pondering
echo -n "Test 4: Ponder Move... "
RESULT=$(echo -e "uci\nisready\nposition startpos\ngo depth 8\nquit" | timeout 60 $ENGINE 2>&1 | grep "bestmove" | grep -c "ponder")
if [ "$RESULT" -ge 1 ]; then
    echo "PASSED ✓"
    ((TESTS_PASSED++))
else
    echo "FAILED ✗"
    ((TESTS_FAILED++))
fi

# Test 5: Hash Option
echo -n "Test 5: Hash Size Option... "
RESULT=$(echo -e "uci" | timeout 5 $ENGINE 2>&1 | grep -c "option name Hash")
if [ "$RESULT" -ge 1 ]; then
    echo "PASSED ✓"
    ((TESTS_PASSED++))
else
    echo "FAILED ✗"
    ((TESTS_FAILED++))
fi

# Test 6: Contempt Option
echo -n "Test 6: Contempt Option... "
RESULT=$(echo -e "uci" | timeout 5 $ENGINE 2>&1 | grep -c "option name Contempt")
if [ "$RESULT" -ge 1 ]; then
    echo "PASSED ✓"
    ((TESTS_PASSED++))
else
    echo "FAILED ✗"
    ((TESTS_FAILED++))
fi

# Test 7: Mate in 1
echo -n "Test 7: Mate in 1 Detection... "
RESULT=$(echo -e "uci\nisready\nposition fen 8/8/8/4k3/8/8/4Q3/4K3 w - - 0 1\ngo depth 5\nquit" | timeout 30 $ENGINE 2>&1 | grep -c "score mate")
if [ "$RESULT" -ge 1 ]; then
    echo "PASSED ✓"
    ((TESTS_PASSED++))
else
    echo "FAILED ✗"
    ((TESTS_FAILED++))
fi

# Test 8: FEN Parsing
echo -n "Test 8: FEN Position Parsing... "
RESULT=$(echo -e "uci\nisready\nposition fen r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3\ngo depth 5\nquit" | timeout 30 $ENGINE 2>&1 | grep -c "bestmove")
if [ "$RESULT" -ge 1 ]; then
    echo "PASSED ✓"
    ((TESTS_PASSED++))
else
    echo "FAILED ✗"
    ((TESTS_FAILED++))
fi

# Test 9: Move Parsing
echo -n "Test 9: Move Sequence Parsing... "
RESULT=$(echo -e "uci\nisready\nposition startpos moves e2e4 e7e5 g1f3\ngo depth 5\nquit" | timeout 30 $ENGINE 2>&1 | grep -c "bestmove")
if [ "$RESULT" -ge 1 ]; then
    echo "PASSED ✓"
    ((TESTS_PASSED++))
else
    echo "FAILED ✗"
    ((TESTS_FAILED++))
fi

# Test 10: Search Speed (NPS)
echo -n "Test 10: Search Speed (NPS)... "
NPS=$(echo -e "uci\nisready\nposition startpos\ngo depth 12\nquit" | timeout 60 $ENGINE 2>&1 | grep "depth 12" | grep -oP "nps \K[0-9]+")
if [ ! -z "$NPS" ] && [ "$NPS" -gt 50000 ]; then
    echo "PASSED ✓ (${NPS} nps)"
    ((TESTS_PASSED++))
else
    echo "FAILED ✗ (NPS: ${NPS:-0})"
    ((TESTS_FAILED++))
fi

echo ""
echo "=================================================="
echo "  Test Results: $TESTS_PASSED passed, $TESTS_FAILED failed"
echo "=================================================="

if [ $TESTS_FAILED -eq 0 ]; then
    echo "  All tests PASSED! Engine is working correctly."
    exit 0
else
    echo "  Some tests FAILED. Check engine functionality."
    exit 1
fi
