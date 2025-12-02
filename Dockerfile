FROM alpine:latest

LABEL org.opencontainers.image.source=https://github.com/flarkflarkflark/AudioRestorationVST
LABEL org.opencontainers.image.description="Audio Restoration Suite - VST3 and Standalone binaries"
LABEL org.opencontainers.image.licenses=MIT

# Install utilities
RUN apk add --no-cache tar gzip

# Create directory for binaries
RUN mkdir -p /opt/audio-restoration

# Copy build artifacts (these will be available during GitHub Actions)
COPY build/AudioRestoration_artefacts/Release/ /opt/audio-restoration/

WORKDIR /opt/audio-restoration

CMD ["sh"]
