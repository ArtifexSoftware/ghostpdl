switch(window.location.protocol) {
    case 'http:':
    case 'https:':
        // remote file
        document.getElementById("searchSite").style.display = "block";
        break;
    case 'file:':
        // local file
        document.getElementById("searchSite").style.display = "none";
        break;
    default:

    break;
}
