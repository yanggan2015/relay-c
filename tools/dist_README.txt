relay-c - portable package (binary only)
========================================

This folder is a binary distribution. Source code is closed-source / private.

详细中文使用说明请打开同目录：

  使用说明.txt
  （或 USAGE.zh-CN.txt，内容相同）

1. Config
   - Copy boards.json.example to boards.json and edit serial ports
   - Or let run.bat create boards.json for you on first start

2. Start
   - Double-click run.bat
   - Or:
       relay-c.exe
       relay-c.exe -c boards.json
       relay-c.exe -n
       relay-c.exe help
       relay-c.exe version

3. Browser
   http://127.0.0.1:18053/          control
   http://127.0.0.1:18053/config   config editor
   http://127.0.0.1:18053/docs     API docs

License / expiry
  Each build is valid for 90 days from compile time.
  Download new Windows builds:
    https://github.com/yanggan2015/relay-c-releases/releases
  Contact: yanggan2015@foxmail.com

Keep exe and sibling DLLs together.
See VERSION.txt for stamp and expiry.
See 使用说明.txt for full user guide.
