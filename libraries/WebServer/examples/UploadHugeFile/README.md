# Upload Huge File To SD Over Http

This project is an example of an HTTP server designed to facilitate the transfer of large files using the PUT method, in accordance with RFC specifications.

### Example cURL Command

```bash
curl -X PUT -T ./my-file.mp3 http://esp-ip/upload/my-file.mp3
```

## Resources

- RFC HTTP/1.0 - Additional Request Methods - PUT : [Link](https://datatracker.ietf.org/doc/html/rfc1945#appendix-D.1.1)
