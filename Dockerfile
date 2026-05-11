FROM python:3.12-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build g++ git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN pip install --no-cache-dir scikit-build-core pybind11 numpy \
    && pip install --no-cache-dir . \
    && pip cache purge

FROM python:3.12-slim

COPY --from=builder /usr/local/lib/python3.12/site-packages /usr/local/lib/python3.12/site-packages
COPY --from=builder /usr/local/bin/bionetgen /usr/local/bin/bionetgen

RUN pip install --no-cache-dir numpy pandas matplotlib

WORKDIR /work
ENTRYPOINT ["bionetgen"]
CMD ["--help"]
