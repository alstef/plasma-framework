include "console.iol"

inputPort PrinterService {
    Location: "socket://localhost:10000"
    Protocol: sodep
    RequestResponse:
        printInput
}

main
{
    printInput(input)() {
        println@Console(input)()
    }
}
