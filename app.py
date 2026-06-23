from flask import Flask, request, jsonify
import socket
import time

app = Flask(__name__)

# This is our fake "Main Hard Drive Database"
SLOW_DB = {
    "AAPL": "150.25",
    "GOOGL": "2800.50",
    "TSLA": "900.10"
}

def ask_cpp_server(command_string):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect(('127.0.0.1', 1234))
            
            cmd_bytes = command_string.encode('utf-8')
            length = len(cmd_bytes)
            s.sendall(length.to_bytes(4, byteorder='little') + cmd_bytes)
            
            reply_len_bytes = s.recv(4)
            if not reply_len_bytes:
                return "(nil)"
            reply_len = int.from_bytes(reply_len_bytes, byteorder='little')
            
            reply_bytes = s.recv(reply_len)
            return reply_bytes.decode('utf-8').strip()
    except Exception as e:
        print("Server error:", e)
        return "Error"

@app.route('/api/direct', methods=['GET'])
def get_direct():
    ticker = request.args.get('ticker')
    time.sleep(1.5) # Simulate slow database
    price = SLOW_DB.get(ticker, "Not Found")
    return jsonify({"price": price, "source": "Slow DB"})

@app.route('/api/cache', methods=['GET'])
def get_cached():
    ticker = request.args.get('ticker')
    key = f"stock:{ticker}"
    
    cached_price = ask_cpp_server(f"GET {key}")
    
    if cached_price != "(nil)" and cached_price != "Error":
        return jsonify({"price": cached_price, "source": "C++ Cache"})
    
    time.sleep(1.5) 
    price = SLOW_DB.get(ticker, "Not Found")
    
    if price != "Not Found":
        ask_cpp_server(f"SET {key} {price} 30")
        
    return jsonify({"price": price, "source": "Slow DB (Saved to Cache)"})

# --- NEW: Route to add a stock to the Slow DB ---
@app.route('/api/add', methods=['POST'])
def add_stock():
    data = request.json
    ticker = data.get('ticker').upper()
    price = data.get('price')
    
    # Save it to our dictionary
    SLOW_DB[ticker] = str(price)
    
    return jsonify({"status": "success", "message": f"Added {ticker} to the database!"})

@app.after_request
def add_cors_headers(response):
    response.headers['Access-Control-Allow-Origin'] = '*'
    response.headers['Access-Control-Allow-Headers'] = 'Content-Type'
    return response

if __name__ == '__main__':
    print("Python Web Server running on http://127.0.0.1:5000")
    app.run(port=5000)