#ifndef WEB_CONTENT_H
#define WEB_CONTENT_H

const char obsahWeb[] = 
R"html(
<html lang="cs">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi edit</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 20px;
            background-color: #f4f4f4;
        }
        h1 {
            text-align: center;
            margin-bottom: 30px;
            color: #333;
        }
        form {
            max-width: 300px;
            margin: 0 auto;
            padding: 20px;
            background-color: #fff;
            border-radius: 8px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        }
        label {
            font-weight: bold;
            color: #333;
        }
        input[type="text"],
        input[type="password"],
        input[type="submit"] {
            width: 100%;
            padding: 10px;
            margin-bottom: 10px;
            border-radius: 5px;
            border: 1px solid #ccc;
            box-sizing: border-box;
        }
        input[type="submit"] {
            background-color: #007bff;
            color: white;
            border: none;
            cursor: pointer;
        }
        input[type="submit"]:hover {
            background-color: #0056b3;
        }
        .success-message {
            color: green;
            text-align: center;
            font-weight: bold;
        }
        .additional-info {
            color: black;
            text-align: center;
            font-weight: bold; 
        }
    </style>
</head>
<body>
    <h1>Změna Wi-Fi připojovacích parametrů</h1>
    <form action="/" method="post">
        <label for="ssid">Název sítě (SSID):</label><br>
        <input type="text" id="ssid" name="ssid"><br>
        <label for="password">Heslo:</label><br>
        <input type="password" id="password" name="password">
        <input type="submit" value="Potvrdit" onclick="redirectPage2()">
    </form>
    <p class="success-message" id="successMessage" style="display: none;">WiFi parametry byly úspěšně přidány!</p>
    <p class="additional-info" id="additionalInfo" style="display: none;">Rádio bude restartováno</p>
    <script>
        const form = document.querySelector("form");
        const successMessage = document.getElementById("successMessage");
        const additionalInfo = document.getElementById("additionalInfo");
        form.addEventListener('submit', function (e) {
            // You need to remove this line to allow form submission
            // e.preventDefault();
            form.style.display = 'none';
            successMessage.style.display = 'block';
            additionalInfo.style.display = 'block';
        });
    </script>
</body>
</html>
)html";
#endif
