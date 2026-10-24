FROM nginx:alpine

# The reverse-proxy configuration and the public embed page are versioned with
# the image, so a released image only needs the TLS material mounted at runtime
# (/etc/nginx/ssl). Locally, docker-compose.yml keeps mounting the same files
# from the checkout for live editing.
COPY nginx.conf /etc/nginx/nginx.conf
COPY html/ /etc/nginx/html/

EXPOSE 80 443
