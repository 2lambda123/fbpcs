FROM python:3.8-slim

# Set BUNDLE_ID ENV
ARG FBPCS_BUNDLE_ID
ENV FBPCS_BUNDLE_ID=${FBPCS_BUNDLE_ID:-0}
RUN echo FBPCS_BUNDLE_ID=$FBPCS_BUNDLE_ID

# Install Dependencies
RUN apt-get update
RUN apt-get install -y software-properties-common
RUN apt-get update && apt-get install -y \
    awscli \
    gcc \
    git \
    unzip \
    vim

# Create the pcs user
RUN useradd -ms /bin/bash pcs

# Switch to pcs user
USER pcs

RUN mkdir /home/pcs/pl_coordinator_env
COPY fbpcs/ /home/pcs/pl_coordinator_env/fbpcs/
WORKDIR /home/pcs/pl_coordinator_env

# Switch back to root to ensure root user run compatible
USER root

# Narrow down the permission to only the workdir for pcs user
RUN chown -R pcs /home/pcs/pl_coordinator_env/
RUN chmod -R u+rw /tmp
RUN chmod -R u+rwx /home/pcs/pl_coordinator_env

RUN python3.8 -m pip install -r fbpcs/pip_requirements.txt

CMD ["/bin/bash"]
