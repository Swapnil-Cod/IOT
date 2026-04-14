from datetime import datetime, timedelta
import configparser
import json
import jwt
import secrets
from functools import wraps

from flask import Flask, request, jsonify, abort
from flask_sqlalchemy import SQLAlchemy
from werkzeug.exceptions import BadRequest, NotFound
from werkzeug.security import generate_password_hash, check_password_hash
from hmac import compare_digest as safe_str_cmp

# Flask-Admin + WTForms
from flask_admin import Admin, expose, AdminIndexView
from flask_admin.contrib.sqla import ModelView
from wtforms import PasswordField
from flask import Response

# --- App setup ---
app = Flask(__name__)

config = configparser.ConfigParser()
config.read("config.ini")

app.config["SECRET_KEY"] = config.get("flask", "SECRET_KEY", fallback="dev-secret-key")
app.config["JWT_SECRET_KEY"] = config.get("flask", "JWT_SECRET_KEY", fallback="jwt-secret-key-change-in-production")
app.config["JWT_ACCESS_TOKEN_EXPIRES_MINUTES"] = config.getint("flask", "JWT_ACCESS_TOKEN_EXPIRES_MINUTES", fallback=60)
app.config["JWT_REFRESH_TOKEN_EXPIRES_DAYS"] = config.getint("flask", "JWT_REFRESH_TOKEN_EXPIRES_DAYS", fallback=30)
app.config["ADMIN_USERNAME"] = config.get("flask", "ADMIN_USERNAME", fallback="admin")
app.config["ADMIN_PASSWORD"] = config.get("flask", "ADMIN_PASSWORD", fallback="adminpass")
app.config["SQLALCHEMY_DATABASE_URI"] = config.get("flask", "SQLALCHEMY_DATABASE_URI", fallback="sqlite:///db.sqlite3")
app.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = config.getboolean("flask", "SQLALCHEMY_TRACK_MODIFICATIONS", fallback=False)


db = SQLAlchemy(app)

# --- JWT Helper Functions ---
def generate_access_token(user_id, phone):
    """Generate short-lived JWT access token for authenticated user"""
    expiration = datetime.utcnow() + timedelta(minutes=app.config["JWT_ACCESS_TOKEN_EXPIRES_MINUTES"])
    payload = {
        "user_id": user_id,
        "phone": phone,
        "type": "access",
        "exp": expiration
    }
    token = jwt.encode(payload, app.config["JWT_SECRET_KEY"], algorithm="HS256")
    return token

def generate_refresh_token(user_id, device_info=None):
    """Generate long-lived refresh token and store in database"""
    # Generate secure random token
    token = secrets.token_urlsafe(64)

    # Calculate expiration
    expires_at = datetime.utcnow() + timedelta(days=app.config["JWT_REFRESH_TOKEN_EXPIRES_DAYS"])

    # Store in database
    refresh_token = RefreshToken(
        token=token,
        user_id=user_id,
        device_info=device_info,
        expires_at=expires_at
    )
    db.session.add(refresh_token)
    db.session.commit()

    return token

def revoke_refresh_token(token):
    """Revoke a refresh token"""
    refresh_token = RefreshToken.query.filter_by(token=token).first()
    if refresh_token:
        refresh_token.revoked = True
        db.session.commit()
        return True
    return False

def validate_refresh_token(token):
    """Validate refresh token and return user if valid"""
    refresh_token = RefreshToken.query.filter_by(token=token).first()

    if not refresh_token:
        return None, "Invalid refresh token"

    if refresh_token.revoked:
        return None, "Refresh token has been revoked"

    if refresh_token.expires_at < datetime.utcnow():
        return None, "Refresh token has expired"

    user = DeviceRecord.query.get(refresh_token.user_id)
    if not user:
        return None, "User not found"

    return user, None

def token_required(f):
    """Decorator to protect routes with JWT authentication"""
    @wraps(f)
    def decorated(*args, **kwargs):
        token = None

        # Check for token in Authorization header
        if "Authorization" in request.headers:
            auth_header = request.headers["Authorization"]
            try:
                # Expected format: "Bearer <token>"
                token = auth_header.split(" ")[1]
            except IndexError:
                return jsonify({
                    "status": "error",
                    "message": "Invalid authorization header format. Use: Bearer <token>"
                }), 401

        if not token:
            return jsonify({
                "status": "error",
                "message": "Authentication token is missing"
            }), 401

        try:
            # Decode and verify token
            payload = jwt.decode(token, app.config["JWT_SECRET_KEY"], algorithms=["HS256"])
            current_user_id = payload["user_id"]
            current_user_phone = payload["phone"]

            # Verify user still exists in database
            current_user = DeviceRecord.query.get(current_user_id)
            if not current_user or current_user.phone != current_user_phone:
                return jsonify({
                    "status": "error",
                    "message": "Invalid authentication token"
                }), 401

            # Pass user info to the route
            kwargs["current_user"] = current_user

        except jwt.ExpiredSignatureError:
            return jsonify({
                "status": "error",
                "message": "Token has expired. Please login again."
            }), 401
        except jwt.InvalidTokenError:
            return jsonify({
                "status": "error",
                "message": "Invalid token. Please login again."
            }), 401

        return f(*args, **kwargs)

    return decorated

# --- Database Models ---
class DeviceRecord(db.Model):
    __tablename__ = "device_records"

    id = db.Column(db.Integer, primary_key=True)
    device_id = db.Column(db.String(64), unique=False, nullable=True)  # optional
    username = db.Column(db.String(64), nullable=True)
    phone = db.Column(db.String(32), unique=True, nullable=False, index=True)

    # Password is now hashed using werkzeug's pbkdf2:sha256
    user_password = db.Column(db.String(256), nullable=True)

    ssid = db.Column(db.String(64), nullable=True)
    ssid_password = db.Column(db.String(128), nullable=True)

    # NEW: Store user's IoT configuration (rooms, devices, topics)
    config = db.Column(db.Text, nullable=True)  # Stores JSON string

    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False)
    updated_at = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)

    # Relationship to refresh tokens
    refresh_tokens = db.relationship('RefreshToken', backref='user', lazy=True, cascade='all, delete-orphan')


class RefreshToken(db.Model):
    __tablename__ = "refresh_tokens"

    id = db.Column(db.Integer, primary_key=True)
    token = db.Column(db.String(256), unique=True, nullable=False, index=True)
    user_id = db.Column(db.Integer, db.ForeignKey('device_records.id'), nullable=False)
    device_info = db.Column(db.String(256), nullable=True)  # Store device/client info
    expires_at = db.Column(db.DateTime, nullable=False)
    revoked = db.Column(db.Boolean, default=False, nullable=False)
    created_at = db.Column(db.DateTime, default=datetime.utcnow, nullable=False)

def require_json(keys):
    if not request.is_json:
        raise BadRequest("Expected JSON body.")
    data = request.get_json(silent=True) or {}
    missing = [k for k in keys if k not in data]
    if missing:
        raise BadRequest(f"Missing fields: {', '.join(missing)}")
    return data


# --- API endpoints (unchanged, only formatting) ---

# --- Create/Upsert Profile (by phone) ---
@app.route("/api/users", methods=["POST"])
def save_profile():
    """
    POST JSON:
    {
      "phone": "<number>",           # required (key)
      "username": "<name>",          # required
      "password": "<pwd>",           # required (will be hashed)
      "device_id": "<id>"            # optional
    }
    """
    data = require_json(["phone", "username", "password"])
    phone = str(data["phone"]).strip()
    username = str(data["username"]).strip()
    password = str(data["password"])
    device_id = (str(data.get("device_id", "")).strip() or None)

    # Hash the password
    hashed_password = generate_password_hash(password)

    rec = DeviceRecord.query.filter_by(phone=phone).first()
    if rec is None:
        rec = DeviceRecord(
            phone=phone,
            username=username,
            user_password=hashed_password,
            device_id=device_id
        )
        db.session.add(rec)
    else:
        rec.username = username
        rec.user_password = hashed_password
        rec.device_id = device_id

    db.session.commit()
    return jsonify({
        "status": "ok",
        "message": "Registration successful"
    }), 201


# --- Login ---
@app.route("/api/login", methods=["POST"])
def login():
    """
    POST JSON:
    {
      "phone": "<number>",     # required
      "password": "<pwd>",     # required
      "device_info": "<info>"  # optional - device identifier for token management
    }
    """
    data = require_json(["phone", "password"])
    phone = str(data["phone"]).strip()
    password = str(data["password"])
    device_info = data.get("device_info", None)

    rec = DeviceRecord.query.filter_by(phone=phone).first()

    # Check if user exists
    if not rec:
        return jsonify({
            "status": "error",
            "message": "User not found. Please register first."
        }), 404

    # Check if password matches (support both hashed and plain text for migration)
    password_valid = False
    if rec.user_password.startswith("pbkdf2:sha256:"):
        # Hashed password
        password_valid = check_password_hash(rec.user_password, password)
    else:
        # Plain text password (for backward compatibility during migration)
        password_valid = rec.user_password == password

        # Migrate to hashed password on successful login
        if password_valid:
            rec.user_password = generate_password_hash(password)
            db.session.commit()

    if not password_valid:
        return jsonify({
            "status": "error",
            "message": "Incorrect password."
        }), 401

    # Login successful - generate both access and refresh tokens
    access_token = generate_access_token(rec.id, rec.phone)
    refresh_token = generate_refresh_token(rec.id, device_info)

    return jsonify({
        "status": "success",
        "message": "Login successful",
        "access_token": access_token,
        "refresh_token": refresh_token,
        "token_type": "Bearer",
        "expires_in": app.config["JWT_ACCESS_TOKEN_EXPIRES_MINUTES"] * 60,  # in seconds
        "user": {
            "id": rec.id,
            "username": rec.username,
            "device_id": rec.device_id
        }
    }), 200


# --- Refresh Token ---
@app.route("/api/token/refresh", methods=["POST"])
def refresh():
    """
    POST JSON:
    {
      "refresh_token": "<token>"  # required
    }
    """
    data = require_json(["refresh_token"])
    refresh_token = data["refresh_token"]

    # Validate refresh token
    user, error = validate_refresh_token(refresh_token)

    if error:
        return jsonify({
            "status": "error",
            "message": error
        }), 401

    # Generate new access token
    access_token = generate_access_token(user.id, user.phone)

    return jsonify({
        "status": "success",
        "access_token": access_token,
        "token_type": "Bearer",
        "expires_in": app.config["JWT_ACCESS_TOKEN_EXPIRES_MINUTES"] * 60
    }), 200


# --- Logout (Revoke Refresh Token) ---
@app.route("/api/logout", methods=["POST"])
@token_required
def logout(current_user):
    """
    POST JSON:
    {
      "refresh_token": "<token>"  # required - the refresh token to revoke
    }
    """
    data = require_json(["refresh_token"])
    refresh_token = data["refresh_token"]

    # Revoke the refresh token
    success = revoke_refresh_token(refresh_token)

    if success:
        return jsonify({
            "status": "success",
            "message": "Logged out successfully"
        }), 200
    else:
        return jsonify({
            "status": "error",
            "message": "Invalid refresh token"
        }), 400


# --- Get client by phone ---
@app.route("/api/users", methods=["GET"])
@token_required
def get_client_by_phone(current_user):
    """
    GET /api/client?phone=<number>
    """
    phone = request.args.get("phone", "").strip()
    if not phone:
        raise BadRequest("Query param 'phone' is required.")
    rec = DeviceRecord.query.filter_by(phone=phone).first()
    if not rec:
        raise NotFound("Client not found.")
    return jsonify({
        "id": rec.id,
        "phone": rec.phone,
        "username": rec.username,
        "device_id": rec.device_id,
        "ssid": rec.ssid,
        "config": json.loads(rec.config) if rec.config else None,
        "created_at": rec.created_at.isoformat() if rec.created_at else None,
        "updated_at": rec.updated_at.isoformat() if rec.updated_at else None
    })


# --- Update client by phone (profile + Wi-Fi; partial) ---
@app.route("/api/users", methods=["PUT"])
@token_required
def update_client_by_phone(current_user):
    """
    PUT JSON: {
      "phone": "<number>",            # required
      "username": "<name>",           # optional
      "password": "<pwd>",            # optional
      "device_id": "<id>",            # optional
      "ssid": "<ssid>",               # optional
      "ssid_password": "<pwd>"        # optional
    }
    """
    data = require_json(["phone"])
    phone = str(data["phone"]).strip()

    rec = DeviceRecord.query.filter_by(phone=phone).first()
    if not rec:
        raise NotFound("Client not found.")

    if "username" in data:
        rec.username = str(data["username"]).strip()
    if "password" in data:
        # Hash the new password
        rec.user_password = generate_password_hash(str(data["password"]))
    if "device_id" in data:
        rec.device_id = (str(data["device_id"]).strip() or None)
    if "ssid" in data:
        rec.ssid = str(data["ssid"]).strip()
    if "ssid_password" in data:
        rec.ssid_password = str(data["ssid_password"])
    if "config" in data:
        rec.config = json.dumps(data["config"])

    db.session.commit()
    return jsonify({
        "status": "ok",
        "message": "Update successful"
    })


# --- Delete client by phone ---
@app.route("/api/users", methods=["DELETE"])
@token_required
def delete_client_by_phone(current_user):
    """
    DELETE JSON: { "phone": "<number>" }
    """
    data = require_json(["phone"])
    phone = str(data["phone"]).strip()
    rec = DeviceRecord.query.filter_by(phone=phone).first()
    if not rec:
        raise NotFound("Client not found.")
    db.session.delete(rec)
    db.session.commit()
    return jsonify({"status": "ok", "deleted_phone": phone})


# ---------------------------
# --- Flask-Admin Setup -----
# ---------------------------

def check_admin_auth():
    """Return True if request provides valid basic auth credentials (username/password)."""
    auth = request.authorization
    if not auth:
        return False
    expected_user = app.config.get("ADMIN_USERNAME")
    expected_pw = app.config.get("ADMIN_PASSWORD")
    # safe_str_cmp helps avoid timing attacks
    return safe_str_cmp(auth.username, expected_user) and safe_str_cmp(auth.password, expected_pw)

class AuthenticatedModelView(ModelView):
    """
    ModelView that requires HTTP Basic Auth (checks app.config ADMIN_USERNAME/PASSWORD).
    """

    def is_accessible(self):
        try:
            return check_admin_auth()
        except Exception:
            return False

    def inaccessible_callback(self, name, **kwargs):
        # Return 401 with WWW-Authenticate so browsers prompt
        return Response(
            "Authentication required.",
            401,
            {"WWW-Authenticate": 'Basic realm="Login Required"'}
        )


class MyAdminIndexView(AdminIndexView):
    @expose('/')
    def index(self):
        if not check_admin_auth():
            return Response(
                "Authentication required.",
                401,
                {"WWW-Authenticate": 'Basic realm="Login Required"'}
            )
        return super(MyAdminIndexView, self).index()


# instantiate admin interface
admin = Admin(app, name="Device Admin", index_view=MyAdminIndexView(), template_mode="bootstrap4")

# Configure ModelView for DeviceRecord
class DeviceRecordAdminView(AuthenticatedModelView):
    # columns to show in the list view
    column_list = ("id", "phone", "username", "device_id", "ssid", "config", "created_at", "updated_at")
    column_searchable_list = ("phone", "username", "device_id", "ssid")
    column_filters = ("created_at", "username", "device_id")
    can_export = True

    # form: show these columns in create/edit
    form_columns = ("phone", "username", "user_password", "device_id", "ssid", "ssid_password", "config")

    # Use password fields for password-like columns in the admin form
    form_extra_fields = {
        "user_password": PasswordField("User Password"),
        "ssid_password": PasswordField("SSID Password"),
    }

    # Make phone required in the form (flask-admin will still show errors if missing)
    form_args = {
        "phone": {"label": "Phone", "validators": []},
    }

# register view
admin.add_view(DeviceRecordAdminView(DeviceRecord, db.session, category="Models"))

# ---------------------------
# --- Run app --------------
# ---------------------------
if __name__ == "__main__":
    with app.app_context():
        db.create_all()
    # dev server
    app.run(host="0.0.0.0", port=8001, debug=True)
