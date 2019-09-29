// First, checks if it isn't implemented yet.
if (!String.prototype.format) {
    String.prototype.format = function() {
        var args = arguments;
        return this.replace(/{(\d+)}/g, function(match, number) { 
            return typeof args[number] != 'undefined'
                ? args[number]
                : match
            ;
        });
    };
}

var releaseURL = 'https://api.github.com/repos/sle118/squeezelite-esp32/releases';
var recovery = false;
var enableAPTimer = true;
var enableStatusTimer = true;
var commandHeader = 'squeezelite -b 500:2000 -d all=info ';
var pname, ver, otapct;
var blockAjax = false;

var apList = null;
var selectedSSID = "";
var refreshAPInterval = null; 
var checkStatusInterval = null;

var StatusIntervalActive = false;
var RefreshAPIIntervalActive = false;

var output = '';

function stopCheckStatusInterval(){
    if(checkStatusInterval != null){
        clearTimeout(checkStatusInterval);
        checkStatusInterval = null;
    }
    StatusIntervalActive = false;
}

function stopRefreshAPInterval(){
    if(refreshAPInterval != null){
        clearTimeout(refreshAPInterval);
        refreshAPInterval = null;
    }
    RefreshAPIIntervalActive = false;
}

function startCheckStatusInterval(){
    StatusIntervalActive = true;
    checkStatusInterval = setTimeout(checkStatus, 3000);
}

function startRefreshAPInterval(){
    RefreshAPIIntervalActive = true;
    refreshAPInterval = setTimeout(refreshAP, 2800);
}

function RepeatCheckStatusInterval(){
    if(StatusIntervalActive)
        startCheckStatusInterval();
}

function RepeatRefreshAPInterval(){
    if(RefreshAPIIntervalActive)
        startRefreshAPInterval();
}

$(document).ready(function(){
    $("#wifi-status").on("click", ".ape", function() {
        $( "#wifi" ).slideUp( "fast", function() {});
        $( "#connect-details" ).slideDown( "fast", function() {});
    });

    $("#manual_add").on("click", ".ape", function() {
        selectedSSID = $(this).text();
        $( "#ssid-pwd" ).text(selectedSSID);
        $( "#wifi" ).slideUp( "fast", function() {});
        $( "#connect_manual" ).slideDown( "fast", function() {});
        $( "#connect" ).slideUp( "fast", function() {});

        //update wait screen
        $( "#loading" ).show();
        $( "#connect-success" ).hide();
        $( "#connect-fail" ).hide();
    });

    $("#wifi-list").on("click", ".ape", function() {
        selectedSSID = $(this).text();
        $( "#ssid-pwd" ).text(selectedSSID);
        $( "#wifi" ).slideUp( "fast", function() {});
        $( "#connect_manual" ).slideUp( "fast", function() {});
        $( "#connect" ).slideDown( "fast", function() {});

        //update wait screen
        $( "#loading" ).show();
        $( "#connect-success" ).hide();
        $( "#connect-fail" ).hide();		
    });

    $("#cancel").on("click", function() {
        selectedSSID = "";
        $( "#connect" ).slideUp( "fast", function() {});
        $( "#connect_manual" ).slideUp( "fast", function() {});
        $( "#wifi" ).slideDown( "fast", function() {});
    });

    $("#manual_cancel").on("click", function() {
        selectedSSID = "";
        $( "#connect" ).slideUp( "fast", function() {});
        $( "#connect_manual" ).slideUp( "fast", function() {});
        $( "#wifi" ).slideDown( "fast", function() {});
    });

    $("#join").on("click", function() {
        performConnect();
    });

    $("#manual_join").on("click", function() {
        performConnect($(this).data('connect'));
    });

    $("#ok-details").on("click", function() {
        $( "#connect-details" ).slideUp( "fast", function() {});
        $( "#wifi" ).slideDown( "fast", function() {});

    });

    $("#ok-credits").on("click", function() {
        $( "#credits" ).slideUp( "fast", function() {});
        $( "#app" ).slideDown( "fast", function() {});
    });

    $("#acredits").on("click", function(event) {
        event.preventDefault();
        $( "#app" ).slideUp( "fast", function() {});
        $( "#credits" ).slideDown( "fast", function() {});
    });

    $("#ok-connect").on("click", function() {
        $( "#connect-wait" ).slideUp( "fast", function() {});
        $( "#wifi" ).slideDown( "fast", function() {});
    });

    $("#disconnect").on("click", function() {
        $( "#connect-details-wrap" ).addClass('blur');
        $( "#diag-disconnect" ).slideDown( "fast", function() {});
    });

    $("#no-disconnect").on("click", function() {
        $( "#diag-disconnect" ).slideUp( "fast", function() {});
        $( "#connect-details-wrap" ).removeClass('blur');
    });

    $("#yes-disconnect").on("click", function() {
        stopCheckStatusInterval();
        selectedSSID = "";

        $( "#diag-disconnect" ).slideUp( "fast", function() {});
        $( "#connect-details-wrap" ).removeClass('blur');

        $.ajax({
            url: '/connect.json',
            dataType: 'json',
            method: 'DELETE',
            cache: false,
            data: { 'timestamp': Date.now()}
        });

        startCheckStatusInterval();

        $( "#connect-details" ).slideUp( "fast", function() {});
        $( "#wifi" ).slideDown( "fast", function() {})
    });

    $("#autoexec-cb").on("click", function() {
        autoexec = (this.checked)?1:0;
        $.ajax({
            url: '/config.json',
            dataType: 'json',
            method: 'POST',
            cache: false,
            headers: { "X-Custom-autoexec": autoexec },
            data: { 'timestamp': Date.now() }
        });
        console.log('sent config JSON with headers:', autoexec);
        console.log('now triggering reboot');
        $.ajax({
            url: '/reboot.json',
            dataType: 'json',
            method: 'POST',
            cache: false,
            data: { 'timestamp': Date.now()}
        });
    });

    $("#save-autoexec1").on("click", function() {
        autoexec1 = $("#autoexec1").val();

        $.ajax({
            url: '/config.json',
            dataType: 'json',
            method: 'POST',
            cache: false,
            headers: { "X-Custom-autoexec1": autoexec1 },
            data: { 'timestamp': Date.now() }
        });
        console.log('sent config JSON with headers:', autoexec1);
    });

    $("#save-gpio").on("click", function() {
        var headers = {};
        $("input.gpio").each(function() {
            var id = $(this)[0].id;
            var pin = $(this).val();
            if (pin != '') {
                headers[id] = pin;
            }
        });
        $.ajax({
            url: '/config.json',
            dataType: 'json',
            method: 'POST',
            cache: false,
            headers: JSON.stringify(headers),
            data: { 'timestamp': Date.now() }
        });
        console.log('sent config JSON with headers:', JSON.stringify(headers));
    });

    $("#flash").on("click", function() {
        var url = $("#fwurl").val();
        $.ajax({
            url: '/config.json',
            dataType: 'json',
            method: 'POST',
            cache: false,
            headers: { "X-Custom-fwurl": url },
            data: { 'timestamp': Date.now() }
        });
    });

    $("#generate-command").on("click", function() {
        var commandLine = commandHeader + '-n ' + $("#player").val();

        if (output == 'bt') {
            commandLine += ' -o "BT -n \'' + $("#btsink").val() + '\'" -R -Z 192000';
        } else if (output == 'spdif') {
            commandLine += ' -o SPDIF -R -Z 192000';
        } else {
            commandLine += ' -o I2S';
        }
        if ($("#optional").val() != '') {
            commandLine += ' ' + $("#optional").val();
        }
        $("#autoexec1").val(commandLine);
    });	

    $('[name=audio]').on("click", function(){
        if (this.id == 'bt') {
            $("#btsinkdiv").show(200);
            output = 'bt';
        } else if (this.id == 'spdif') {
            $("#btsinkdiv").hide(200);
            output = 'spdif';
        } else {
            $("#btsinkdiv").hide(200);
            output = 'i2s';
        }
    });

    $('#fwcheck').on("click", function(){
        $("#releaseTable").html("");
        $.getJSON(releaseURL, function(data) {
            data.forEach(function(release) {
                var url = '';
                release.assets.forEach(function(asset) {
                    if (asset.name.match(/\.bin$/)) {
                        url = asset.browser_download_url;
                    }
                });
                var [ver, idf, cfg, branch] = release.name.split('#');
                var body = release.body.replace(/\\n/ig, "<br />").replace(/\'/ig, "\"");
                    var [date, time] = release.created_at.split('T');
                    if (ver.match(/esp-idf/)) return; //TODO delete
                    $("#releaseTable").append(
                        "<tr>"+
                        "<td data-toggle='tooltip' title='"+body+"'>"+ver+"</td>"+
                        "<td>"+idf+"</td>"+
                        "<td>"+date+"</td>"+
                        "<td>"+cfg+"</td>"+
                        "<td>"+branch+"</td>"+
                        "<td><input id='generate-command' type='button' class='btn btn-success' value='Select' data-url='"+url+"' onclick='setURL(this);' /></td>"+
                        "</tr>"
                    );
            });
        })
                .fail(function() {
                    alert("failed to fetch release history!");
                });
    });

        //first time the page loads: attempt to get the connection status and start the wifi scan
        refreshAP();
        getConfig();

        //start timers
        startCheckStatusInterval();
        startRefreshAPInterval();

        $('[data-toggle="tooltip"]').tooltip({
            html: true,
            placement : 'right',
        });
});

    function setURL(button) {
        var url = button.dataset.url;
        $("#fwurl").val(url);

        $('[data-url^="http"]').addClass("btn-success").removeClass("btn-danger");
        $('[data-url="'+url+'"]').addClass("btn-danger").removeClass("btn-success");
    }

    function performConnect(conntype){
        //stop the status refresh. This prevents a race condition where a status 
        //request would be refreshed with wrong ip info from a previous connection
        //and the request would automatically shows as succesful.
        stopCheckStatusInterval();

        //stop refreshing wifi list
        stopRefreshAPInterval();

        var pwd;
        if (conntype == 'manual') {
            //Grab the manual SSID and PWD
            selectedSSID=$('#manual_ssid').val();
            pwd = $("#manual_pwd").val();
        }else{
            pwd = $("#pwd").val();
        }
        //reset connection 
        $( "#loading" ).show();
        $( "#connect-success" ).hide();
        $( "#connect-fail" ).hide();

        $( "#ok-connect" ).prop("disabled",true);
        $( "#ssid-wait" ).text(selectedSSID);
        $( "#connect" ).slideUp( "fast", function() {});
        $( "#connect_manual" ).slideUp( "fast", function() {});
        $( "#connect-wait" ).slideDown( "fast", function() {});


        $.ajax({
            url: '/connect.json',
            dataType: 'json',
            method: 'POST',
            cache: false,
            headers: { 'X-Custom-ssid': selectedSSID, 'X-Custom-pwd': pwd },
            data: { 'timestamp': Date.now()}
        });


        //now we can re-set the intervals regardless of result
        startCheckStatusInterval();
        startRefreshAPInterval();
    }

    function rssiToIcon(rssi){
        if(rssi >= -60){
            return 'w0';
        }
        else if(rssi >= -67){
            return 'w1';
        }
        else if(rssi >= -75){
            return 'w2';
        }
        else{
            return 'w3';
        }
    }

    function refreshAP(){
        if (!enableAPTimer) return;
        $.getJSON( "/ap.json", function( data ) {
            if(data.length > 0){
                //sort by signal strength
                data.sort(function (a, b) {
                    var x = a["rssi"]; var y = b["rssi"];
                    return ((x < y) ? 1 : ((x > y) ? -1 : 0));
                });
                apList = data;
                refreshAPHTML(apList);
            }
        });
    }

    function refreshAPHTML(data){
        var h = "";
        data.forEach(function(e, idx, array) {
            h += '<div class="ape{0}"><div class="{1}"><div class="{2}">{3}</div></div></div>'.format(idx === array.length - 1?'':' brdb', rssiToIcon(e.rssi), e.auth==0?'':'pw',e.ssid);
            h += "\n";
        });

        $( "#wifi-list" ).html(h)
    }

    function checkStatus(){
        RepeatCheckStatusInterval();
        if (!enableStatusTimer) return;
        if (blockAjax) return;
        blockAjax = true;
        $.getJSON( "/status.json", function( data ) {
            if(data.hasOwnProperty('ssid') && data['ssid'] != ""){
                if(data["ssid"] === selectedSSID){
                    //that's a connection attempt
                    if(data["urc"] === 0){
                        //got connection
                        $("#connected-to span").text(data["ssid"]);
                        $("#connect-details h1").text(data["ssid"]);
                        $("#ip").text(data["ip"]);
                        $("#netmask").text(data["netmask"]);
                        $("#gw").text(data["gw"]);
                        $("#wifi-status").slideDown( "fast", function() {});
                        $("span#foot-wifi").html(", SSID: <strong>"+data["ssid"]+"</strong>, IP: <strong>"+data["ip"]+"</strong>");

                        //unlock the wait screen if needed
                        $( "#ok-connect" ).prop("disabled",false);

                        //update wait screen
                        $( "#loading" ).hide();
                        $( "#connect-success" ).append("<p>Your IP address now is: " + text(data["ip"]) + "</p>");
                        $( "#connect-success" ).show();
                        $( "#connect-fail" ).hide();

                        enableAPTimer = false;
                        if (!recovery) enableStatusTimer = false;
                    }
                    else if(data["urc"] === 1){
                        //failed attempt
                        $("#connected-to span").text('');
                        $("#connect-details h1").text('');
                        $("#ip").text('0.0.0.0');
                        $("#netmask").text('0.0.0.0');
                        $("#gw").text('0.0.0.0');
                        $("span#foot-wifi").html("");

                        //don't show any connection
                        $("#wifi-status").slideUp( "fast", function() {});

                        //unlock the wait screen
                        $( "#ok-connect" ).prop("disabled",false);

                        //update wait screen
                        $( "#loading" ).hide();
                        $( "#connect-fail" ).show();
                        $( "#connect-success" ).hide();

                        enableAPTimer = true;
                        enableStatusTimer = true;
                    }
                }
                else if(data.hasOwnProperty('urc') && data['urc'] === 0){
                    //ESP32 is already connected to a wifi without having the user do anything
                    if( !($("#wifi-status").is(":visible")) ){
                        $("#connected-to span").text(data["ssid"]);
                        $("#connect-details h1").text(data["ssid"]);
                        $("#ip").text(data["ip"]);
                        $("#netmask").text(data["netmask"]);
                        $("#gw").text(data["gw"]);
                        $("#wifi-status").slideDown( "fast", function() {});
                        $("span#foot-wifi").html(", SSID: <strong>"+data["ssid"]+"</strong>, IP: <strong>"+data["ip"]+"</strong>");
                    }
                    enableAPTimer = false;
                    if (!recovery) enableStatusTimer = false;
                }
            }
            else if(data.hasOwnProperty('urc') && data['urc'] === 2){
                //that's a manual disconnect
                if($("#wifi-status").is(":visible")){
                    $("#wifi-status").slideUp( "fast", function() {});
                    $("span#foot-wifi").html("");
                }
                enableAPTimer = true;
                enableStatusTimer = true;
            }
            if(data.hasOwnProperty('project_name') && data['project_name'] != ''){
                pname = data['project_name'];
            }
            if(data.hasOwnProperty('version') && data['version'] != ''){
                ver = data['version'];
                $("span#foot-fw").html("fw: <strong>"+ver+"</strong>, mode: <strong>"+pname+"</strong>");
            }
            if(data.hasOwnProperty('ota_pct') && data['ota_pct'] != 0){
                otapct = data['ota_pct'];
                $('.progress-bar').css('width', otapct+'%').attr('aria-valuenow', otapct);
                $('.progress-bar').html(otapct+'%');
            }
            blockAjax = false;
        })
        .fail(function() {
            //don't do anything, the server might be down while esp32 recalibrates radio
        });
    }

    function getConfig() {
        $.getJSON("/config.json", function(data) {
            if (data.hasOwnProperty('autoexec')) {
                if (data["autoexec"] === 1) {
                    console.log('turn on autoexec');
                    $("#autoexec-cb")[0].checked=true;
                } else {
                    console.log('turn off autoexec');
                    $("#autoexec-cb")[0].checked=false;
                }
            }
            if (data.hasOwnProperty('recovery')) {  //move to status!
                if (data["recovery"] === 1) {
                    recovery = true;
                    $("#otadiv").show();
                    enableStatusTimer = true;
                } else {
                    recovery = false;
                    $("#otadiv").hide();
                    enableStatusTimer = false;
                }
            }
            if (data.hasOwnProperty('list')) {
                data.list.forEach(function(line) {
                    let key = Object.keys(line)[0];
                    let val = Object.values(line)[0];
                    console.log(key, val);
                    if (key == 'autoexec1') {
                        $("#autoexec1").val(val);
                    }
                });
            }
        })
        .fail(function() {
            console.log("failed to fetch config!");
        });
    }
