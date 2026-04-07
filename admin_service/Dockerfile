FROM python:3.12-slim

WORKDIR /app

COPY admin_service/requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir -r /app/requirements.txt

COPY admin_service /app/admin_service

CMD ["uvicorn", "admin_service.app.main:app", "--host", "0.0.0.0", "--port", "8010"]
