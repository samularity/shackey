# shackey [![build status](https://github.com/samularity/shackey/actions/workflows/build.yml/badge.svg)](https://github.com/samularity/shackey/actions/workflows/build.yml)

### a simple ESP32-C3 SSH Client  

It's main (and only goal) is to open or close the [shackspace portal](https://github.com/shackspace/portal300) with a button press on a keychain device. 



### How it works

- deepsleep
- Read two buttons (and Reset Pin) to open or close
- connect to wifi
- open a SSH session with your private key on a remote server
- provide a webserver to upload or delete your key

```mermaid
graph TD
    NodeStart((deepsleep)) --> wakeup -- wakup-source --> NodeWAKE{ } 

    wakeup -- enable --> NodeTimer{{45sec watchdog timer}} -. Timeout .-> NodeZZZ


    NodeWAKE -->|BtnRst + Btn1| NodeK(["setup-mode"]) -->  NodeClient
    NodeClient --> NodeClient
    NodeClient["wait for client"] --> connected -- disable --> NodeTimer


    NodeWAKE -->|Btn1| NodeJ([open]) -- set cmd=open--> NodeWIFI
    NodeWAKE -->|Btn2| NodeI([close]) -- set cmd=close--> NodeWIFI

    NodeWIFI["connect to wifi"] -- connected --> NodeCMD["ssh cmd"] -- got response --> NodeZZZ
    NodeWIFI-->NodeWIFI
    NodeCMD --> NodeCMD

    NodeWAKE -->|BtnRst| NodeZZZ  

    NodeZZZ((deepsleep))
```


### Software components used
- libssh2 latest version is used without any modifications.  
- It's arduino free without any outdated dependencies or additional overhead.  
- Its based on the latest espidf supported by platformio.



### thx & refs

- https://github.com/libssh2/libssh2
- https://github.com/espressif/esp-idf/tree/master/examples/
- https://www.ewan.cc/?q=node/157
- https://github.com/ewpa/LibSSH-ESP32
- https://github.com/nopnop2002/esp-idf-ssh-client
