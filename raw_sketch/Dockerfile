FROM node:24-alpine AS builder
WORKDIR /app
COPY package.json ./
RUN npm install --production

FROM node:24-alpine
WORKDIR /app
COPY --from=builder /app/node_modules ./node_modules
COPY app.js ./
COPY public/ ./public/
EXPOSE 3000
CMD ["node", "app.js"]