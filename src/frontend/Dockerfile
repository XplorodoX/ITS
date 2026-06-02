# Build stage
FROM node:20-alpine AS builder

WORKDIR /app

# Copy package files
COPY package*.json ./

# Install dependencies
RUN npm ci

# Copy application code
COPY . .

# WebSocket URL of the MQTT broker, as seen FROM THE BEAMER'S BROWSER (not from
# inside the Docker network). NEXT_PUBLIC_* is inlined at build time, so it must
# be provided here as a build arg — a runtime env var would be ignored.
ARG NEXT_PUBLIC_MQTT_URL=ws://localhost:9001
ENV NEXT_PUBLIC_MQTT_URL=$NEXT_PUBLIC_MQTT_URL

# Build the Next.js application
RUN npm run build

# Production stage
FROM node:20-alpine

WORKDIR /app

# Copy package files
COPY package*.json ./

# Install production dependencies only
RUN npm ci --only=production

# Copy built application from builder
COPY --from=builder /app/.next ./.next
COPY --from=builder /app/next.config.mjs ./next.config.mjs
# Copy public directory if it exists (optional)
RUN mkdir -p public

# Expose port
EXPOSE 3000

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=40s --retries=3 \
  CMD node -e "require('http').get('http://localhost:3000', (r) => {if (r.statusCode !== 200) throw new Error(r.statusCode)})"

# Start the application
CMD ["npm", "start"]
