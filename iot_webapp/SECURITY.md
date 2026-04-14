# Security Configuration Guide

## Initial Setup

1. **Copy the example secrets file:**
   ```bash
   cp secrets.example.json secrets.json
   ```

2. **Edit `secrets.json` with your actual credentials:**
   ```json
   {
       "mqtt": {
           "broker": "your-mqtt-broker-ip",
           "port": 1883,
           "username": "your-mqtt-username",
           "password": "your-mqtt-password",
           "client_id": "flask-mqtt-dashboard"
       },
       "api": {
           "base_url": "http://your-api-server-ip:8001"
       },
       "flask": {
           "secret_key": "GENERATE-A-STRONG-RANDOM-SECRET-KEY-HERE"
       }
   }
   ```

3. **Generate a strong Flask secret key:**
   ```bash
   python -c "import secrets; print(secrets.token_hex(32))"
   ```
   Copy the output and paste it as the `secret_key` value.

## Security Notes

### ⚠️ Current Security Model

#### JWT Token Storage
- **JWT tokens are stored in Flask sessions (browser cookies)**
- Flask sessions are **signed but NOT encrypted by default**
- Tokens can be read by anyone with access to the cookie
- This is **acceptable for development** but has limitations for production

#### Is this secure enough?

**For Development/Home Use:** ✅ Yes
- If you trust your local network
- If you're the only user
- If you use HTTPS (recommended)

**For Production/Public Access:** ⚠️ Needs Improvement
- Consider server-side sessions (Redis/Database)
- Implement CSRF protection
- Use HTTPS-only cookies
- Add rate limiting

### 🔒 What is Protected

✅ **secrets.json is in .gitignore** - Won't be committed to Git
✅ **All sensitive config centralized** - MQTT, API URL, secret keys
✅ **JWT authentication** - API requests require valid tokens
✅ **Session-based auth** - Users must login

### 🚨 What to Improve for Production

1. **Use HTTPS** - Encrypt all traffic
   ```python
   # In app.py, add:
   app.config['SESSION_COOKIE_SECURE'] = True  # HTTPS only
   app.config['SESSION_COOKIE_HTTPONLY'] = True  # No JavaScript access
   app.config['SESSION_COOKIE_SAMESITE'] = 'Lax'  # CSRF protection
   ```

2. **Server-side sessions** - Store tokens on server, not in cookies
   ```bash
   pip install flask-session redis
   ```

3. **Environment variables** - Don't commit secrets.json anywhere
   ```bash
   export FLASK_SECRET_KEY="your-secret-key"
   ```

4. **Token refresh** - Handle expired JWT tokens gracefully

5. **Rate limiting** - Prevent brute force attacks
   ```bash
   pip install flask-limiter
   ```

## Alternative: Storing Tokens in secrets.json

**❌ NOT RECOMMENDED** - Here's why:

1. **Static tokens** - JWT tokens expire after 24 hours, you'd need to manually update
2. **One token per user** - Can't support multiple users
3. **Security risk** - If secrets.json leaks, attacker has full access
4. **No logout** - Users can't invalidate their sessions

**Current approach (session cookies) is better** because:
- ✅ Tokens are per-session and auto-expire
- ✅ Logout works properly
- ✅ Supports multiple users
- ✅ Tokens refresh on login

## Best Practices

1. **Never commit secrets.json** - It's in .gitignore, keep it that way
2. **Use strong passwords** - For MQTT and user accounts
3. **Rotate secrets regularly** - Change Flask secret key periodically
4. **Use HTTPS in production** - Encrypt all traffic
5. **Monitor logs** - Watch for unauthorized access attempts
6. **Backup secrets.json** - Store securely offline

## Questions?

If you need help implementing production-grade security, let me know!
