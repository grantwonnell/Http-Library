# HTTP Library
Lightweight HTTP library in C for client side only

# Info

* HTTP library in itself is 160 lines (I was proud of this) 
* Code is from a bigger project so its why theres utilties for some library functions 
* I tried to think through how to make the sockets non blocking or at least stop hangups but it seemed impossible because we return the response from the direct function 
* I also tried figuring how to not statically define the header length but was too lazy and I wanted to keep the structure definition in the config

# Usage

    GET
    httpresponse_t *resp = HTTP(&(httpconfig_t){
        .host = inet_addr("1.1.1.1"),
        .port = htons(80),
        
        .path = "index.html",
        .data = NULL,
        .output = "/root/index.html",
        .method = "GET",
        .version = "1.1",
    
        .headers_len = 3,
        .headers = {
            {"Host", "216.18.189.81"},
            {"User-Agent", "Wget"},
            {"Connection", "close"},
        },
    });

    POST
    httpresponse_t *resp = HTTP(&(httpconfig_t){
        .host = inet_addr("1.1.1.1"),
        .port = htons(80),
        
        .path = "/index.php",
        .data = "var1=hello&var2=world",
        .output = NULL,
        .method = "POST",
        .version = "1.1",
    
        .headers_len = 4,
        .headers = {
            {"Host", "216.18.189.81"},
            {"User-Agent", "Wget"},
            {"Connection", "close"},
            {"Conent-Type", "application/x-www-form-urlencoded"},
        },
    });
