# Use an official Python runtime as a parent image
FROM python:3.13-slim-buster

# Set the working directory in the container
WORKDIR /app

# Copy the requirements.txt file and install dependencies (if any)
COPY requirements.txt .
RUN pip install -r requirements.txt

# Copy the rest of the application code
COPY . .

# Specify the command to run the application
CMD ["python", "main.py"]
