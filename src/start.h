char *start_page= R"=====(
<html>
  <head>
    <title>ESP8266 Access Point</title>
    <style>
      html, body {
      display: flex;
      justify-content: center;
      font-family: Roboto, Arial, sans-serif;
      font-size: 15px;
      }
      form {
      border: 5px solid #f1f1f1;
      }
      input[type=text], input[type=password] {
      width: 100%;
      padding: 16px 8px;
      margin: 8px 0;
      display: inline-block;
      border: 1px solid #ccc;
      box-sizing: border-box;
      }
      button {
      background-color: #8ebf42;
      color: white;
      padding: 14px 0;
      margin: 10px 0;
      border: none;
      cursor: grabbing;
      width: 100%;
      }
      h1 {
      text-align:center;
      font-size:18;
      }
      button:hover {
      opacity: 0.8;
      }
      .formcontainer {
      text-align: left;
      margin: 24px 50px 12px;
      }
      .container {
      padding: 16px 0;
      text-align:left;
      }
      span.password {
      float: right;
      padding-top: 0;
      padding-right: 15px;
      }
      /* Change styles for span on extra small screens */
      @media screen and (max-width: 300px) {
      span.password {
      display: block;
      float: none;
      }
    </style>
  </head>
  <body>
    <form action="/answer.html" method="post">
      <h1>ESP8266 Access Point</h1>
      <div class="formcontainer">
      <hr/>
      <div class="container">
        <label for="UID"<strong>UID</strong></label>
        <input type="text" placeholder="Choose and enter UID for this device" name="UID" required>
        <label for="SSID"><strong>SSID</strong></label>
        <input type="text" placeholder="Enter SSID" name="SSID" required>
        <label for="password"><strong>Password</strong></label>
        <input type="password" placeholder="Enter Password" name="password" required>
      </div>
      <button type="submit">Submit</button>
    </form>
  </body>
</html>
)=====";