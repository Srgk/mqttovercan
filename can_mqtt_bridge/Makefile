# Makefile for running Python tests

# Configuration
PYTHON ?= python3.13  # Default Python executable, can be overridden with make PYTHON=python3.9

.PHONY: test clean

# Default target
all: test

# Run mypy static type checking
lint:
	$(PYTHON) -m mypy . --strict

# Run all tests in the tests directory
test:
	$(PYTHON) -m unittest discover -s tests -p "test_*.py" -v

# Clean up Python cache files
clean:
	find . -type d -name "__pycache__" -exec rm -r {} +
	find . -type f -name "*.pyc" -delete
