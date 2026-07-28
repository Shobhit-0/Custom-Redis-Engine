from flask import Flask, request, jsonify
import socket
import time

app = Flask(__name__)

SLOW_DB = []

print("Loading slow database... this will take a few seconds.")

# Add 5 million fake stocks to make the list huge
for i in range(5000000):
    SLOW_DB.append({"ticker": f"DUMMY{i}", "price": "1.00"})

SLOW_DB.append({"ticker": "AAPL", "price": "150.25"})
SLOW_DB.append({"ticker": "GOOGL", "price": "2800.50"})
SLOW_DB.append({"ticker": "TSLA", "price": "900.10"})

print("Database loaded!")

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
    
    price = "Not Found"
    for item in SLOW_DB:
        if item["ticker"] == ticker:
            price = item["price"]
            break 
            
    return jsonify({"price": price, "source": "Slow DB"})

@app.route('/api/cache', methods=['GET'])
def get_cached():
    ticker = request.args.get('ticker')
    key = f"stock:{ticker}"
    
    cached_price = ask_cpp_server(f"GET {key}")
    
    if cached_price != "(nil)" and cached_price != "Error":
        return jsonify({"price": cached_price, "source": "C++ Cache"})
    
   
    price = "Not Found"
    for item in SLOW_DB:
        if item["ticker"] == ticker:
            price = item["price"]
            break 
    
    if price != "Not Found":
        ask_cpp_server(f"SET {key} {price} 30")
        
    return jsonify({"price": price, "source": "Slow DB (Saved to Cache)"})


@app.route('/api/add', methods=['POST'])
def add_stock():
    data = request.json
    ticker = data.get('ticker').upper()
    price = data.get('price')
    
    
    SLOW_DB.append({"ticker": ticker, "price": str(price)})
    
    return jsonify({"status": "success", "message": f"Added {ticker} to the database!"})

@app.after_request
def add_cors_headers(response):
    response.headers['Access-Control-Allow-Origin'] = '*'
    response.headers['Access-Control-Allow-Headers'] = 'Content-Type'
    return response

if __name__ == '__main__':
    print("Python Web Server running on http://127.0.0.1:5000")
    app.run(port=5000)
