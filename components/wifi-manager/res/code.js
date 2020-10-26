// First, checks if it isn't implemented yet.
if (!String.prototype.format) {
	String.prototype.format = function() {
		var args = arguments;
		return this.replace(/{(\d+)}/g, function(match, number) {
			return typeof args[number] != 'undefined' ?
				args[number] :
				match;
		});
	};
}
if (!String.prototype.encodeHTML) {
	String.prototype.encodeHTML = function () {
	  return this.replace(/&/g, '&amp;')
				 .replace(/</g, '&lt;')
				 .replace(/>/g, '&gt;')
				 .replace(/"/g, '&quot;')
				 .replace(/'/g, '&apos;');
	};
  }
var nvs_type_t = {
	NVS_TYPE_U8: 0x01,
	/*!< Type uint8_t */
	NVS_TYPE_I8: 0x11,
	/*!< Type int8_t */
	NVS_TYPE_U16: 0x02,
	/*!< Type uint16_t */
	NVS_TYPE_I16: 0x12,
	/*!< Type int16_t */
	NVS_TYPE_U32: 0x04,
	/*!< Type uint32_t */
	NVS_TYPE_I32: 0x14,
	/*!< Type int32_t */
	NVS_TYPE_U64: 0x08,
	/*!< Type uint64_t */
	NVS_TYPE_I64: 0x18,
	/*!< Type int64_t */
	NVS_TYPE_STR: 0x21,
	/*!< Type string */
	NVS_TYPE_BLOB: 0x42,
	/*!< Type blob */
	NVS_TYPE_ANY: 0xff /*!< Must be last */
};
pillcolors = {
	'MESSAGING_INFO' : 'badge-success',
	'MESSAGING_WARNING' : 'badge-warning',
	'MESSAGING_ERROR' : 'badge-danger'
}
var task_state_t = {
	0: "eRunning",
	/*!< A task is querying the state of itself, so must be running. */
	1: "eReady",
	/*!< The task being queried is in a read or pending ready list. */
	2: "eBlocked",
	/*!< The task being queried is in the Blocked state. */
	3: "eSuspended",
	/*!< The task being queried is in the Suspended state, or is in the Blocked state with an infinite time out. */
	4: "eDeleted"

}
var escapeHTML = function(unsafe) {
	return unsafe.replace(/[&<"']/g, function(m) {
		switch (m) {
			case '&':
				return '&amp;';
			case '<':
				return '&lt;';
			case '"':
				return '&quot;';
			default:
				return '&#039;';
		}
	});
};
function setNavColor(stylename){
	$('[name=secnav]').removeClass('bg-secondary bg-warning');
	$("footer.footer").removeClass('bg-secondary bg-warning');
	$("#mainnav").removeClass('bg-secondary bg-warning');
	if(stylename.length>0){
		$('[name=secnav]').addClass(stylename);
		$("footer.footer").addClass(stylename);
		$("#mainnav").addClass(stylename);
	}
}
function handleTemplateTypeRadio(outtype){
	if (outtype == 'bt') {
		$("#btsinkdiv").parent().show(200);
		$("#btsinkdiv").show();
		$('#bt').prop('checked',true);
		o_bt.setAttribute("display", "inline");		
		o_spdif.setAttribute("display", "none");	
		o_i2s.setAttribute("display", "none");			
		output = 'bt';
	} else if (outtype == 'spdif') {
		$("#btsinkdiv").parent().hide(200);
		$("#btsinkdiv").show();
		$('#spdif').prop('checked',true);
		o_bt.setAttribute("display", "none");		
		o_spdif.setAttribute("display", "inline");	
		o_i2s.setAttribute("display", "none");			
		output = 'spdif';
	} else {
		$("#btsinkdiv").parent().hide(200);
		$("#btsinkdiv").show();
		$('#i2s').prop('checked',true);
		o_bt.setAttribute("display", "none");		
		o_spdif.setAttribute("display", "none");	
		o_i2s.setAttribute("display", "inline");			
		output = 'i2s';
	}
}

function handleExceptionResponse(xhr, ajaxOptions, thrownError){
	console.log(xhr.status);
	console.log(thrownError);
	enableStatusTimer=true;
	if (thrownError != '') showLocalMessage(thrownError, 'MESSAGING_ERROR');
}
function HideCmdMessage(cmdname){
	$('#toast_'+cmdname).css('display','none');
	$('#toast_'+cmdname).removeClass('table-success').removeClass('table-warning').removeClass('table-danger').addClass('table-success');
	$('#msg_'+cmdname).html('');
}
function showCmdMessage(cmdname,msgtype, msgtext,append=false){
	color='table-success';
	if (msgtype == 'MESSAGING_WARNING') {
		color='table-warning';
	} else if (msgtype == 'MESSAGING_ERROR') {
		color ='table-danger';
	} 						
	$('#toast_'+cmdname).css('display','block');
	$('#toast_'+cmdname).removeClass('table-success').removeClass('table-warning').removeClass('table-danger').addClass(color);
	escapedtext=escapeHTML(msgtext.substring(0, msgtext.length - 1)).replace(/\n/g, '<br />');
	escapedtext=($('#msg_'+cmdname).html().length>0 && append?$('#msg_'+cmdname).html()+'<br/>':'')+escapedtext;
	$('#msg_'+cmdname).html(escapedtext);
}

var releaseURL = 'https://api.github.com/repos/sle118/squeezelite-esp32/releases';
var recovery = false;
var enableAPTimer = true;
var enableStatusTimer = true;
var commandHeader = 'squeezelite -b 500:2000 -d all=info -C 30 -W';
var pname, ver, otapct, otadsc;
var blockAjax = false;
var blockFlashButton = false;
var dblclickCounter = 0;
var apList = null;
var selectedSSID = "";
var refreshAPInterval = null;
var checkStatusInterval = null;
var messagecount=0;
var messageseverity="MESSAGING_INFO";
var StatusIntervalActive = false;
var RefreshAPIIntervalActive = false;
var LastRecoveryState = null;
var LastCommandsState = null;
var output = '';

Promise.prototype.delay = function(duration) {
	return this.then(function(value) {
	  return new Promise(function(resolve) {
		setTimeout(function() {
		  resolve(value)
		}, duration)
	  })
	}, function(reason) {
	  return new Promise(function(resolve, reject) {
		setTimeout(function() {
		  reject(reason)
		}, duration)
	  })
	})
  }
function stopCheckStatusInterval() {
	if (checkStatusInterval != null) {
		clearTimeout(checkStatusInterval);
		checkStatusInterval = null;
	}
	StatusIntervalActive = false;
}

function stopRefreshAPInterval() {
	if (refreshAPInterval != null) {
		clearTimeout(refreshAPInterval);
		refreshAPInterval = null;
	}
	RefreshAPIIntervalActive = false;
}

function startCheckStatusInterval() {
	StatusIntervalActive = true;
	checkStatusInterval = setTimeout(checkStatus, 3000);
}

function startRefreshAPInterval() {
	RefreshAPIIntervalActive = true;
	refreshAPInterval = setTimeout(refreshAP(false), 4500); // leave enough time for the initial scan
}

function RepeatCheckStatusInterval() {
	if (StatusIntervalActive)
		startCheckStatusInterval();
}

function RepeatRefreshAPInterval() {
	if (RefreshAPIIntervalActive)
		startRefreshAPInterval();
}

function getConfigJson(slimMode) {
	var config = {};
	$("input.nvs").each(function() {
		var key = $(this)[0].id;
		var val = $(this).val();
		if (!slimMode) {
			var nvs_type = parseInt($(this)[0].attributes.nvs_type.nodeValue, 10);
			if (key != '') {
				config[key] = {};
				if (nvs_type == nvs_type_t.NVS_TYPE_U8 ||
					nvs_type == nvs_type_t.NVS_TYPE_I8 ||
					nvs_type == nvs_type_t.NVS_TYPE_U16 ||
					nvs_type == nvs_type_t.NVS_TYPE_I16 ||
					nvs_type == nvs_type_t.NVS_TYPE_U32 ||
					nvs_type == nvs_type_t.NVS_TYPE_I32 ||
					nvs_type == nvs_type_t.NVS_TYPE_U64 ||
					nvs_type == nvs_type_t.NVS_TYPE_I64) {
					config[key].value = parseInt(val);
				} else {
					config[key].value = val;
				}
				config[key].type = nvs_type;
			}
		} else {
			config[key] = val;
		}
	});
	var key = $("#nvs-new-key").val();
	var val = $("#nvs-new-value").val();
	if (key != '') {
		if (!slimMode) {
			config[key] = {};
			config[key].value = val;
			config[key].type = 33;
		} else {
			config[key] = val;
		}
	}
	return config;
}

function onFileLoad(elementId, event) {
	var data = {};
	try {
		data = JSON.parse(elementId.srcElement.result);
	} catch (e) {
		alert('Parsing failed!\r\n ' + e);
	}
	$("input.nvs").each(function() {
		var key = $(this)[0].id;
		var val = $(this).val();
		if (data[key]) {
			if (data[key] != val) {
				console.log("Changed " & key & " " & val & "==>" & data[key]);
				$(this).val(data[key]);
			}
		} else {
			console.log("Value " & key & " missing from file");
		}
	});

}

function onChooseFile(event, onLoadFileHandler) {
	if (typeof window.FileReader !== 'function')
		throw ("The file API isn't supported on this browser.");
	input = event.target;
	if (!input)
		throw ("The browser does not properly implement the event object");
	if (!input.files)
		throw ("This browser does not support the `files` property of the file input.");
	if (!input.files[0])
		return undefined;
	file = input.files[0];
	fr = new FileReader();
	fr.onload = onLoadFileHandler;
	fr.readAsText(file);
	input.value = "";
} 
function delay_reboot(duration,cmdname, ota=false){
	url= (ota?'/reboot_ota.json':'/reboot.json');
	$("tbody#tasks").empty();
	setNavColor('bg-secondary');
	enableStatusTimer=false;
	$("#tasks_sect").css('visibility','collapse');
	Promise.resolve(cmdname).delay(duration).then(function(cmdname) {
		if(cmdname?.length >0){
			showCmdMessage(cmdname,'MESSAGING_WARNING','Rebooting the ESP32.\n',true);
		}
		else {
			showLocalMessage('Rebooting the ESP32.\n','MESSAGING_WARNING')
		}
		console.log('now triggering reboot');
		$.ajax({
			url: this.url,
			dataType: 'text',
			method: 'POST',
			cache: false,
			contentType: 'application/json; charset=utf-8',
			data: JSON.stringify({
				'timestamp': Date.now()
			}),
			error: handleExceptionResponse,
			complete: function(response) {
				console.log('reboot call completed');
				enableStatusTimer=true;
				Promise.resolve(cmdname).delay(6000).then(function(cmdname) {
					if(cmdname?.length >0) HideCmdMessage(cmdname);
					getCommands();
					getConfig();
				});
			}
		});
	});
}
function save_autoexec1(apply){
	showCmdMessage('cfg-audio-tmpl','MESSAGING_INFO',"Saving.\n",false);
	var commandLine = commandHeader + ' -n "' + $("#player").val() + '"';
	if (output == 'bt') {
		if($("#btsinkdiv").val()?.length!=0){
			commandLine += ' -o "BT -n \'' + $("#btsinkdiv").val() + '\'" -Z 192000';
		}
		else {
			showCmdMessage('cfg-audio-tmpl','MESSAGING_ERROR',"BT Sink Name required for output bluetooth.\n",true);
			return;
		}
	} else if (output == 'spdif') {
		commandLine += ' -o SPDIF -Z 192000';
	} else {
		commandLine += ' -o I2S';
	}
	if ($("#optional").val() != '') {
		commandLine += ' ' + $("#optional").val();
	}
	var data = {
		'timestamp': Date.now()
	};
	autoexec = $("#disable-squeezelite").prop('checked') ? "0" : "1";
	data['config'] = {
		autoexec1: { value: commandLine, type: 33 },
		autoexec: { value: autoexec, type: 33 }	
	}

	$.ajax({
		url: '/config.json',
		dataType: 'text',
		method: 'POST',
		cache: false,
		contentType: 'application/json; charset=utf-8',
		data: JSON.stringify(data),
		error: handleExceptionResponse,
		complete: function(response) {
			if(JSON.parse(response?.responseText)?.result == "OK"){
				showCmdMessage('cfg-audio-tmpl','MESSAGING_INFO',"Done.\n",true);
				if (apply) {
					delay_reboot(1500,"cfg-audio-tmpl");
				}
			}
			else if(response.responseText) {
				showCmdMessage('cfg-audio-tmpl','MESSAGING_WARNING',JSON.parse(response.responseText).Result + "\n",true);
			}
			else {
				showCmdMessage('cfg-audio-tmpl','MESSAGING_ERROR',response.responseText+'\n');
			}
			console.log(response.responseText);

		}		
	});
	console.log('sent data:', JSON.stringify(data));
}
$(document).ready(function() {
	// $(".dropdown-item").on("click", function(e){
    //     var linkText = $(e.relatedTarget).text(); // Get the link text
        
    // });
	$("input#show-commands")[0].checked = LastCommandsState == 1 ? true : false;
	$('a[href^="#tab-commands"]').hide();
	$("#load-nvs").click(function() {
		$("#nvsfilename").trigger('click');
	});
	$("#wifi-status").on("click", ".ape", function() {
		$("#wifi").slideUp("fast", function() {});
		$("#connect-details").slideDown("fast", function() {});
	});
	$("#clear-syslog").on("click",function(){
		messagecount=0;
		messageseverity="MESSAGING_INFO";
		$('#msgcnt').text('');
		$("#syslogTable").html('');
	});
	$("#manual_add").on("click", ".ape", function() {
		selectedSSID = $(this).text();
		$("#ssid-pwd").text(selectedSSID);
		$("#wifi").slideUp("fast", function() {});
		$("#connect_manual").slideDown("fast", function() {});
		$("#connect").slideUp("fast", function() {});

		//update wait screen
		$("#loading").show();
		$("#connect-success").hide();
		$("#connect-fail").hide();
	});

	$("#wifi-list").on("click", ".ape", function() {
		selectedSSID = $(this).text();
		$("#ssid-pwd").text(selectedSSID);
		$("#wifi").slideUp("fast", function() {});
		$("#connect_manual").slideUp("fast", function() {});
		$("#connect").slideDown("fast", function() {});

		//update wait screen
		$("#loading").show();
		$("#connect-success").hide();
		$("#connect-fail").hide();
	});

	$("#cancel").on("click", function() {
		selectedSSID = "";
		$("#connect").slideUp("fast", function() {});
		$("#connect_manual").slideUp("fast", function() {});
		$("#wifi").slideDown("fast", function() {});
	});

	$("#manual_cancel").on("click", function() {
		selectedSSID = "";
		$("#connect").slideUp("fast", function() {});
		$("#connect_manual").slideUp("fast", function() {});
		$("#wifi").slideDown("fast", function() {});
	});

	$("#join").on("click", function() {
		performConnect();
	});

	$("#manual_join").on("click", function() {
		performConnect($(this).data('connect'));
	});

	$("#ok-details").on("click", function() {
		$("#connect-details").slideUp("fast", function() {});
		$("#wifi").slideDown("fast", function() {});

	});

	$("#ok-credits").on("click", function() {
		$("#credits").slideUp("fast", function() {});
		$("#app").slideDown("fast", function() {});
	});

	$("#acredits").on("click", function(event) {
		event.preventDefault();
		$("#app").slideUp("fast", function() {});
		$("#credits").slideDown("fast", function() {});
	});

	$("#ok-connect").on("click", function() {
		$("#connect-wait").slideUp("fast", function() {});
		$("#wifi").slideDown("fast", function() {});
	});

	$("#disconnect").on("click", function() {
		$("#connect-details-wrap").addClass('blur');
		$("#diag-disconnect").slideDown("fast", function() {});
	});

	$("#no-disconnect").on("click", function() {
		$("#diag-disconnect").slideUp("fast", function() {});
		$("#connect-details-wrap").removeClass('blur');
	});

	$("#yes-disconnect").on("click", function() {
		stopCheckStatusInterval();
		selectedSSID = "";

		$("#diag-disconnect").slideUp("fast", function() {});
		$("#connect-details-wrap").removeClass('blur');

		$.ajax({
			url: '/connect.json',
			dataType: 'text',
			method: 'DELETE',
			cache: false,
			contentType: 'application/json; charset=utf-8',
			data: JSON.stringify({
				'timestamp': Date.now()
			})

		});

		startCheckStatusInterval();

		$("#connect-details").slideUp("fast", function() {});
		$("#wifi").slideDown("fast", function() {})
	});
	$("input#show-commands").on("click", function() {
		this.checked = this.checked ? 1 : 0;
		if (this.checked) {
			$('a[href^="#tab-commands"]').show();
			LastCommandsState = 1;
		} else {
			LastCommandsState = 0;
			$('a[href^="#tab-commands"]').hide();
		}
	});

	$("input#show-nvs").on("click", function() {
		this.checked = this.checked ? 1 : 0;
		if (this.checked) {
			$('a[href^="#tab-nvs"]').show();
		} else {
			$('a[href^="#tab-nvs"]').hide();
		}

	});
	$("#save-as-nvs").on("click", function() {
		var data = {
			'timestamp': Date.now()
		};
		var config = getConfigJson(true);
		const a = document.createElement("a");
		a.href = URL.createObjectURL(
			new Blob([JSON.stringify(config, null, 2)], {
				type: "text/plain"
			}));
		a.setAttribute("download", "nvs_config" + Date.now() + "json");
		document.body.appendChild(a);
		a.click();
		document.body.removeChild(a);
		console.log('sent config JSON with headers:', JSON.stringify(headers));
		console.log('sent config JSON with data:', JSON.stringify(data));
	});

	$("#save-nvs").on("click", function() {
		var headers = {};
		var data = {
			'timestamp': Date.now()
		};
		var config = getConfigJson(false);
		data['config'] = config;
		$.ajax({
			url: '/config.json',
			dataType: 'text',
			method: 'POST',
			cache: false,
			headers: headers,
			contentType: 'application/json; charset=utf-8',
			data: JSON.stringify(data),
			error: handleExceptionResponse
		});
		console.log('sent config JSON with headers:', JSON.stringify(headers));
		console.log('sent config JSON with data:', JSON.stringify(data));
	});
	$("#fwUpload").on("click", function() {
		var upload_path = "/flash.json";

		if(!recovery) $('#flash-status').text('Rebooting to OTA');

		var fileInput = document.getElementById("flashfilename").files;
		if (fileInput.length == 0) {
			alert("No file selected!");
		} else {
			var file = fileInput[0];
			var xhttp = new XMLHttpRequest();
			xhttp.onreadystatechange = function() {
				if (xhttp.readyState == 4) {
					if (xhttp.status == 200) {
						showLocalMessage(xhttp.responseText, 'MESSAGING_INFO')
					} else if (xhttp.status == 0) {
						showLocalMessage("Upload connection was closed abruptly!", 'MESSAGING_ERROR');
					} else {
						showLocalMessage(xhttp.status + " Error!\n" + xhttp.responseText, 'MESSAGING_ERROR');
					}
				}
			};
			xhttp.open("POST", upload_path, true);
			xhttp.send(file);
		}
		enableStatusTimer = true;
	});
	$("#flash").on("click", function() {
		var data = {
			'timestamp': Date.now()
		};
		if (blockFlashButton) return;
		blockFlashButton = true;
		var url = $("#fwurl").val();
		data['config'] = {
			fwurl: {
				value: url,
				type: 33
			}
		};

		$.ajax({
			url: '/config.json',
			dataType: 'text',
			method: 'POST',
			cache: false,
			contentType: 'application/json; charset=utf-8',
			data: JSON.stringify(data),
			error: handleExceptionResponse
		});
		enableStatusTimer = true;
	});

	$('[name=output-tmpl]').on("click", function() {
		handleTemplateTypeRadio(this.id);
	});

	$('#fwcheck').on("click", function() {
		$("#releaseTable").html("");
		$("#fwbranch").empty();
		$.getJSON(releaseURL, function(data) {
				var i = 0;
				var branches = [];
				data.forEach(function(release) {
					namecomponents=release.name.split('#');
					ver=namecomponents[0];
					idf=namecomponents[1];
					cfg=namecomponents[2];
					branch=namecomponents[3];
					if (!branches.includes(branch)) {
						branches.push(branch);
					}
				});
				var fwb;
				branches.forEach(function(branch) {
					fwb += '<option value="' + branch + '">' + branch + '</option>';
				});
				$("#fwbranch").append(fwb);

				data.forEach(function(release) {
					var url = '';
					release.assets.forEach(function(asset) {
						if (asset.name.match(/\.bin$/)) {
							url = asset.browser_download_url;
						}
					});
					namecomponents = release.name.split('#');
					ver=namecomponents[0];
					idf=namecomponents[1];
					cfg=namecomponents[2];
					branch=namecomponents[3];
					
					var body = release.body;
					body = body.replace(/\'/ig, "\"");
					body = body.replace(/[\s\S]+(### Revision Log[\s\S]+)### ESP-IDF Version Used[\s\S]+/, "$1");
					body = body.replace(/- \(.+?\) /g, "- ");
					var [date, time] = release.created_at.split('T');
					var trclass = (i++ > 6) ? ' hide' : '';
					$("#releaseTable").append(
						"<tr class='release" + trclass + "'>" +
						"<td data-toggle='tooltip' title='" + body + "'>" + ver + "</td>" +
						"<td>" + date + "</td>" +
						"<td>" + cfg + "</td>" +
						"<td>" + idf + "</td>" +
						"<td>" + branch + "</td>" +
						"<td><input type='button' class='btn btn-success' value='Select' data-url='" + url + "' onclick='setURL(this);' /></td>" +
						"</tr>"
					);
				});
				if (i > 7) {
					$("#releaseTable").append(
						"<tr id='showall'>" +
						"<td colspan='6'>" +
						"<input type='button' id='showallbutton' class='btn btn-info' value='Show older releases' />" +
						"</td>" +
						"</tr>"
					);
					$('#showallbutton').on("click", function() {
						$("tr.hide").removeClass("hide");
						$("tr#showall").addClass("hide");
					});
				}
				$("#searchfw").css("display", "inline");
			})
			.fail(function() {
				alert("failed to fetch release history!");
			});
	});

	$('input#searchinput').on("input", function() {
		var s = $('input#searchinput').val();
		var re = new RegExp(s, "gi");
		if (s.length == 0) {
			$("tr.release").removeClass("hide");
		} else if (s.length < 3) {
			$("tr.release").addClass("hide");
		} else {
			$("tr.release").addClass("hide");
			$("tr.release").each(function(tr) {
				$(this).find('td').each(function() {
					if ($(this).html().match(re)) {
						$(this).parent().removeClass('hide');
					}
				});
			});
		}
	});

	$("#fwbranch").change(function(e) {
		var branch = this.value;
		var re = new RegExp('^' + branch + '$', "gi");
		$("tr.release").addClass("hide");
		$("tr.release").each(function(tr) {
			$(this).find('td').each(function() {
				console.log($(this).html());
				if ($(this).html().match(re)) {
					$(this).parent().removeClass('hide');
				}
			});
		});
	});

	$('#boot-button').on("click", function() {
		enableStatusTimer = true;
	});
	$('#reboot-button').on("click", function() {
		enableStatusTimer = true;
	});

	$('#updateAP').on("click", function() {
		refreshAP(true);
		console.log("refresh AP");
	});

	//first time the page loads: attempt to get the connection status and start the wifi scan
	refreshAP(false);
	getConfig();
	getCommands();

	//start timers
	startCheckStatusInterval();
	//startRefreshAPInterval();

	$('[data-toggle="tooltip"]').tooltip({
		html: true,
		placement: 'right',
	});
});

function setURL(button) {
	var url = button.dataset.url;
	$("#fwurl").val(url);

	$('[data-url^="http"]').addClass("btn-success").removeClass("btn-danger");
	$('[data-url="' + url + '"]').addClass("btn-danger").removeClass("btn-success");
}

function performConnect(conntype) {
	//stop the status refresh. This prevents a race condition where a status 
	//request would be refreshed with wrong ip info from a previous connection
	//and the request would automatically shows as succesful.
	stopCheckStatusInterval();

	//stop refreshing wifi list
	stopRefreshAPInterval();

	var pwd;
	var dhcpname;
	if (conntype == 'manual') {
		//Grab the manual SSID and PWD
		selectedSSID = $('#manual_ssid').val();
		pwd = $("#manual_pwd").val();
		dhcpname = $("#dhcp-name2").val();;
	} else {
		pwd = $("#pwd").val();
		dhcpname = $("#dhcp-name1").val();;
	}
	//reset connection 
	$("#loading").show();
	$("#connect-success").hide();
	$("#connect-fail").hide();

	$("#ok-connect").prop("disabled", true);
	$("#ssid-wait").text(selectedSSID);
	$("#connect").slideUp("fast", function() {});
	$("#connect_manual").slideUp("fast", function() {});
	$("#connect-wait").slideDown("fast", function() {});

	$.ajax({
		url: '/connect.json',
		dataType: 'text',
		method: 'POST',
		cache: false,
		//        headers: { 'X-Custom-ssid': selectedSSID, 'X-Custom-pwd': pwd, 'X-Custom-host_name': dhcpname },
		contentType: 'application/json; charset=utf-8',
		data: JSON.stringify({
			'timestamp': Date.now(),
			'ssid': selectedSSID,
			'pwd': pwd,
			'host_name': dhcpname
		}),
		error: handleExceptionResponse
	});

	//now we can re-set the intervals regardless of result
	startCheckStatusInterval();
	startRefreshAPInterval();
}

function rssiToIcon(rssi) {
	if (rssi >= -60) {
		return 'w0';
	} else if (rssi >= -67) {
		return 'w1';
	} else if (rssi >= -75) {
		return 'w2';
	} else {
		return 'w3';
	}
}

function refreshAP(force) {
	if (!enableAPTimer && !force) return;
	$.getJSON("/scan.json", async function(data) {
		await sleep(2000);
		$.getJSON("/ap.json", function(data) {
			if (data.length > 0) {
				//sort by signal strength
				data.sort(function(a, b) {
					var x = a["rssi"];
					var y = b["rssi"];
					return ((x < y) ? 1 : ((x > y) ? -1 : 0));
				});
				apList = data;
				refreshAPHTML(apList);
			}
		});
	});
}

function refreshAPHTML(data) {
	var h = "";
	data.forEach(function(e, idx, array) {
		h += '<div class="ape{0}"><div class="{1}"><div class="{2}">{3}</div></div></div>'.format(idx === array.length - 1 ? '' : ' brdb', rssiToIcon(e.rssi), e.auth == 0 ? '' : 'pw', e.ssid);
		h += "\n";
	});

	$("#wifi-list").html(h)
}
function getMessages() {
	$.getJSON("/messages.json?1", async function(data) {
			for (const msg of data) {
				var msg_age = msg["current_time"] - msg["sent_time"];
				var msg_time = new Date();
				msg_time.setTime(msg_time.getTime() - msg_age);
				switch (msg["class"]) {
					case "MESSAGING_CLASS_OTA":
						//message: "{"ota_dsc":"Erasing flash complete","ota_pct":0}"
						var ota_data = JSON.parse(msg["message"]);
						if (ota_data.hasOwnProperty('ota_pct') && ota_data['ota_pct'] != 0) {
							otapct = ota_data['ota_pct'];
							$('.progress-bar').css('width', otapct + '%').attr('aria-valuenow', otapct);
							$('.progress-bar').html(otapct + '%');
						}
						if (ota_data.hasOwnProperty('ota_dsc') && ota_data['ota_dsc'] != '') {
							otadsc = ota_data['ota_dsc'];
							$("span#flash-status").html(otadsc);
							if (msg.type == "MESSAGING_ERROR" || otapct > 95) {
								blockFlashButton = false;
								enableStatusTimer = true;
							}
						}
						break;
					case "MESSAGING_CLASS_STATS":
						// for task states, check structure : task_state_t
						var stats_data = JSON.parse(msg["message"]);
						console.log(msg_time.toLocaleString() + " - Number of tasks on the ESP32: " + stats_data["ntasks"]);
						console.log(msg_time.toLocaleString() + '\tname' + '\tcpu' + '\tstate' + '\tminstk' + '\tbprio' + '\tcprio' + '\tnum');
						if(stats_data["tasks"]){
							if($("#tasks_sect").css('visibility') =='collapse'){
								$("#tasks_sect").css('visibility','visible');
							}
							var trows="";
							stats_data["tasks"].sort(function(a, b){
								return (b.cpu-a.cpu);
							}).forEach(function(task) {
								console.log(msg_time.toLocaleString() + '\t' + task["nme"] + '\t' + task["cpu"] + '\t' + task_state_t[task["st"]] + '\t' + task["minstk"] + '\t' + task["bprio"] + '\t' + task["cprio"] + '\t' + task["num"]);
								trows+='<tr class="table-primary"><th scope="row">' + task["num"]+ '</th><td>' + task["nme"]  + '</td><td>' + task["cpu"] + '</td><td>' + task_state_t[task["st"]] + '</td><td>' + task["minstk"]+ '</td><td>' + task["bprio"]+ '</td><td>' + task["cprio"] + '</td></tr>'
							});
							$("tbody#tasks").html(trows);
						}
						else if($("#tasks_sect").css('visibility') =='visible'){
								$("tbody#tasks").empty();
								$("#tasks_sect").css('visibility','collapse');
							}
						break;
					case "MESSAGING_CLASS_SYSTEM":
						var r = showMessage(msg,msg_time, msg_age);
						break;
					case "MESSAGING_CLASS_CFGCMD":
						var msgparts=msg["message"].split(/([^\n]*)\n(.*)/gs);
						showCmdMessage(msgparts[1],msg['type'],msgparts[2],true);
						break;
					default:
						break;
				}
			}
		})
		.fail( handleExceptionResponse);
	/*
    Minstk is minimum stack space left
Bprio is base priority
cprio is current priority
nme is name
st is task state. I provided a "typedef" that you can use to convert to text
cpu is cpu percent used
*/
}
function handleRecoveryMode(data){
	if (data.hasOwnProperty('recovery')) {
		if (LastRecoveryState != data["recovery"]) {
			LastRecoveryState = data["recovery"];
			$("input#show-nvs")[0].checked = LastRecoveryState == 1 ? true : false;
		}
		if ($("input#show-nvs")[0].checked) {
			$('a[href^="#tab-nvs"]').show();
		} else {
			$('a[href^="#tab-nvs"]').hide();
		}
		enableStatusTimer = true;
		if (data["recovery"] === 1) {
			recovery = true;
			$("#reboot_ota_nav").show();
			$("#reboot_nav").hide();
			$("#otadiv").show();
			$('#uploaddiv').show();
			$("footer.footer").removeClass('sl');
			setNavColor('bg-warning');
			$("#boot-button").html('Reboot');
			$("#boot-form").attr('action', '/reboot_ota.json');
		} else {
			recovery = false;
			$("#reboot_ota_nav").hide();
			$("#reboot_nav").show();
			$("#otadiv").hide();
			$('#uploaddiv').hide();
			setNavColor('');
			$("footer.footer").addClass('sl');
			$("#boot-button").html('Recovery');
			$("#boot-form").attr('action', '/recovery.json');
		}
	}
}
function handleWifiStatus(data){
	if (data.hasOwnProperty('ssid') && data['ssid'] != "") {
		if (data["ssid"] === selectedSSID) {
			//that's a connection attempt
			if (data["urc"] === 0) {
				//got connection
				$("#connected-to span").text(data["ssid"]);
				$("#connect-details h1").text(data["ssid"]);
				$("#ip").text(data["ip"]);
				$("#netmask").text(data["netmask"]);
				$("#gw").text(data["gw"]);
				$("#wifi-status").slideDown("fast", function() {});
				$("span#foot-wifi").html(", SSID: <strong>" + data["ssid"] + "</strong>, IP: <strong>" + data["ip"] + "</strong>");

				//unlock the wait screen if needed
				$("#ok-connect").prop("disabled", false);

				//update wait screen
				$("#loading").hide();
				$("#connect-success").text("Your IP address now is: " + data["ip"]);
				$("#connect-success").show();
				$("#connect-fail").hide();
				enableAPTimer = false;
			} else if (data["urc"] === 1) {
				//failed attempt
				$("#connected-to span").text('');
				$("#connect-details h1").text('');
				$("#ip").text('0.0.0.0');
				$("#netmask").text('0.0.0.0');
				$("#gw").text('0.0.0.0');
				$("span#foot-wifi").html("");
				//don't show any connection
				$("#wifi-status").slideUp("fast", function() {});
				//unlock the wait screen
				$("#ok-connect").prop("disabled", false);

				//update wait screen
				$("#loading").hide();
				$("#connect-fail").show();
				$("#connect-success").hide();

				enableAPTimer = true;
				enableStatusTimer = true;
			}
		} else if (data.hasOwnProperty('urc') && data['urc'] === 0) {
			//ESP32 is already connected to a wifi without having the user do anything
			if (!($("#wifi-status").is(":visible"))) {
				$("#connected-to span").text(data["ssid"]);
				$("#connect-details h1").text(data["ssid"]);
				$("#ip").text(data["ip"]);
				$("#netmask").text(data["netmask"]);
				$("#gw").text(data["gw"]);
				$("#wifi-status").slideDown("fast", function() {});
				$("span#foot-wifi").html(", SSID: <strong>" + data["ssid"] + "</strong>, IP: <strong>" + data["ip"] + "</strong>");
			}
			enableAPTimer = false;
		}
	} else if (data.hasOwnProperty('urc') && data['urc'] === 2) {
		//that's a manual disconnect
		if ($("#wifi-status").is(":visible")) {
			$("#wifi-status").slideUp("fast", function() {});
			$("span#foot-wifi").html("");
		}
		enableAPTimer = true;
		enableStatusTimer = true;
	}
}

function checkStatus() {
	RepeatCheckStatusInterval();
	if (!enableStatusTimer) return;
	if (blockAjax) return;
	blockAjax = true;
	getMessages();
	$.getJSON("/status.json", function(data) {
		handleRecoveryMode(data);
		handleWifiStatus(data);
		if (data.hasOwnProperty('project_name') && data['project_name'] != '') {
			pname = data['project_name'];
		}
		if (data.hasOwnProperty('version') && data['version'] != '') {
			ver = data['version'];
			$("span#foot-fw").html("fw: <strong>" + ver + "</strong>, mode: <strong>" + pname + "</strong>");
		} else {
			$("span#flash-status").html('');
		}
		if (data.hasOwnProperty('Voltage')) {
			var voltage = data['Voltage'];
			var layer;

			/* Assuming Li-ion 18650s as a power source, 3.9V per cell, or above is treated
				as full charge (>75% of capacity).  3.4V is empty. The gauge is loosely
				following the graph here:
					https://learn.adafruit.com/li-ion-and-lipoly-batteries/voltages
				using the 0.2C discharge profile for the rest of the values.
			*/

			if (voltage > 0) {
				if (inRange(voltage, 5.8, 6.8) || inRange(voltage, 8.8, 10.2)) {
					layer = bat0;
				} else if (inRange(voltage, 6.8, 7.4) || inRange(voltage, 10.2, 11.1)) {
					layer = bat1;
				} else if (inRange(voltage, 7.4, 7.5) || inRange(voltage, 11.1, 11.25)) {
					layer = bat2;
				} else if (inRange(voltage, 7.5, 7.8) || inRange(voltage, 11.25, 11.7)) {
					layer = bat3;	
				} else {
					layer = bat4;
				}
				layer.setAttribute("display","inline");
			}
		}
		if (data.hasOwnProperty('Jack')) {
			var jack = data['Jack'];
			if (jack == '1') {
				o_jack.setAttribute("display", "inline");
			}
		}
		blockAjax = false;
	})
	.fail(function(xhr, ajaxOptions, thrownError) {
		handleExceptionResponse(xhr, ajaxOptions, thrownError);
		blockAjax = false;
	});
}
function runCommand(button, reboot) {
	cmdstring = button.attributes.cmdname.value;
	showCmdMessage(button.attributes.cmdname.value,'MESSAGING_INFO',"Executing.",false);
	fields = document.getElementById("flds-" + cmdstring);
	cmdstring += ' ';
	if (fields) {
		allfields = fields.querySelectorAll("select,input");
		for (i = 0; i < allfields.length; i++) {
			attr = allfields[i].attributes;
			qts = '';
			opt = '';
			isSelect = allfields[i].attributes?.class?.value == "custom-select";
			if ((isSelect && allfields[i].selectedIndex != 0) || !isSelect) {
				if (attr.longopts.value !== "undefined") {
					opt += '--' + attr.longopts.value;
				} else if (attr.shortopts.value !== "undefined") {
					opt = '-' + attr.shortopts.value;
				}

				if (attr.hasvalue.value == "true") {
					if (allfields[i].value != '') {
						qts = (/\s/.test(allfields[i].value)) ? '"' : '';
						cmdstring += opt + ' '+  qts + allfields[i].value + qts + ' ';
					}
				} else {
					// this is a checkbox
					if (allfields[i].checked) cmdstring += opt + ' ';
				}
			}
		}
	}
	console.log(cmdstring);

	var data = {
		'timestamp': Date.now()
	};
	data['command'] = cmdstring;

	$.ajax({
		url: '/commands.json',
		dataType: 'text',
		method: 'POST',
		cache: false,
		contentType: 'application/json; charset=utf-8',
		data: JSON.stringify(data),
		error: handleExceptionResponse,
		complete: function(response) {
			//var returnedResponse = JSON.parse(response.responseText);
			console.log(response.responseText);
			if (response.responseText && JSON.parse(response.responseText).Result == "Success" && reboot) {
				delay_reboot(2500,button.attributes.cmdname.value);
			}
		}
	});
	enableStatusTimer = true;
}

function getCommands() {
	$.getJSON("/commands.json", function(data) {
			console.log(data);
			data.commands.forEach(function(command) {
				if ($("#flds-" + command.name).length == 0) {
					cmd_parts = command.name.split('-');
					isConfig = cmd_parts[0]=='cfg';
					targetDiv= '#tab-' + cmd_parts[0]+'-'+cmd_parts[1];
					innerhtml = '';
					//innerhtml+='<tr class="table-light"><td>'+(isConfig?'<h1>':'');
					innerhtml += '<div class="card text-white bg-primary mb-3"><div class="card-header">' + escapeHTML(command.help).replace(/\n/g, '<br />') + '</div><div class="card-body">';
					innerhtml += '<fieldset id="flds-' + command.name + '">';
					if (command.hasOwnProperty("argtable")) {
						command.argtable.forEach(function(arg) {
							placeholder = arg?.datatype || '';
							ctrlname = command.name + '-' + arg.longopts;
							curvalue = data.values?. [command.name]?. [arg.longopts];

							var attributes = 'hasvalue=' + arg.hasvalue + ' ';
							//attributes +='datatype="'+arg.datatype+'" ';
							attributes += 'longopts="' + arg.longopts + '" ';
							attributes += 'shortopts="' + arg.shortopts + '" ';
							attributes += 'checkbox=' + arg.checkbox + ' ';
							attributes += 'cmdname="' + command.name + '" ';
							attributes += 'id="' + ctrlname + '" name="' + ctrlname + '" hasvalue="' + arg.hasvalue + '"   ';
							extraclass =  ((arg.mincount>0)?'bg-success':'');
							if(arg.glossary == 'hidden'){
								attributes += ' style="visibility: hidden;"';
							}
							if (arg.checkbox) {
								innerhtml += '<div class="form-check"><label class="form-check-label">';
								innerhtml += '<input type="checkbox" ' + attributes + ' class="form-check-input '+extraclass+'" value="" >' + arg.glossary.encodeHTML() + '<small class="form-text text-muted">Previous value: ' + (curvalue?"Checked":"Unchecked") + '</small></label>';
							} else {
								innerhtml += '<div class="form-group" ><label for="' + ctrlname + '">' + arg.glossary.encodeHTML() + '</label>';
								if (placeholder.includes('|')) {
									extraclass = (placeholder.startsWith('+')? ' multiple ':'');
									placeholder = placeholder.replace('<', '').replace('=', '').replace('>', '');
									innerhtml += '<select ' + attributes + ' class="form-control '+extraclass+'"';
									placeholder = '--|' + placeholder;
									placeholder.split('|').forEach(function(choice) {
										innerhtml += '<option >' + choice + '</option>';
									});
									innerhtml += '</select>';
								} else {
									innerhtml += '<input type="text" class="form-control '+extraclass+'" placeholder="' + placeholder + '" ' + attributes + '>';
								}
								innerhtml += '<small class="form-text text-muted">Previous value: ' + (curvalue || '') + '</small>';
							}
							innerhtml += '</div>';
						});
					}
					innerhtml +='<div style="margin-top: 16px;">';
					innerhtml += '<div class="toast show" role="alert" aria-live="assertive" aria-atomic="true" style="display: none;" id="toast_'+command.name+'">';
					innerhtml += '<div class="toast-header"><strong class="mr-auto">Result</strong><button type="button" class="ml-2 mb-1 close" data-dismiss="toast" aria-label="Close" onclick="$(this).parent().parent().hide()">';
					innerhtml += '<span aria-hidden="true">×</span></button></div><div class="toast-body" id="msg_'+command.name+'"></div></div>'
					if (isConfig) {
						innerhtml += '<button type="submit" class="btn btn-info" id="btn-save-' + command.name + '" cmdname="' + command.name + '" onclick="runCommand(this,false)">Save</button>';
						innerhtml += '<button type="submit" class="btn btn-warning" id="btn-commit-' + command.name + '" cmdname="' + command.name + '" onclick="runCommand(this,true)">Apply</button>';
					} else {
						innerhtml += '<button type="submit" class="btn btn-success" id="btn-run-' + command.name + '" cmdname="' + command.name + '" onclick="runCommand(this,false)">Execute</button>';
					}
					innerhtml+= '</div></fieldset></div></div>';
					if (isConfig) {
						$(targetDiv).append(innerhtml);
					} else {
						$("#commands-list").append(innerhtml);
					}
				}
			});

			data.commands.forEach(function(command) {
				$('[cmdname='+command.name+']:input').val('');
				$('[cmdname='+command.name+']:checkbox').prop('checked',false);
				if (command.hasOwnProperty("argtable")) {
					command.argtable.forEach(function(arg) {
						ctrlselector = '#' + command.name + '-' + arg.longopts;
						ctrlValue = data.values?.[command.name]?.[arg.longopts];
						if (arg.checkbox) {
							$(ctrlselector)[0].checked = ctrlValue;
						} else {
							if(ctrlValue!=undefined) $(ctrlselector).val( ctrlValue ).change();
							if ($(ctrlselector)[0].value.length == 0 && (arg?.datatype || '').includes('|')) {
								$(ctrlselector)[0].value = '--';
							}
						}
					});
				}
			});

		})
		.fail(function(xhr, ajaxOptions, thrownError) {
			handleExceptionResponse(xhr, ajaxOptions, thrownError);
			$("#commands-list").empty();
			blockAjax = false;
		});
}

function getConfig() {
	$.getJSON("/config.json", function(entries) {
		$("#nvsTable tr").remove();
		data = entries.hasOwnProperty('config') ? entries.config : entries;
			Object.keys(data).sort().forEach(function(key, i) {
				if (data.hasOwnProperty(key)) {
					val = data[key].value;
					if (key == 'autoexec') {
						if (data["autoexec"].value === "0") {
							$("#disable-squeezelite")[0].checked = true;
						} else {
							$("#disable-squeezelite")[0].checked = false;
						}
					} else if (key == 'autoexec1') {						
						var re = /-o\s?(["][^"]*["]|[^-]+)/g;
						var m = re.exec(val);
						if (m[1].toUpperCase().startsWith('I2S')) {
							handleTemplateTypeRadio('i2s');
						} else if (m[1].toUpperCase().startsWith('SPDIF')) {
							handleTemplateTypeRadio('spdif');
						} else if (m[1].toUpperCase().startsWith('"BT')) {
							handleTemplateTypeRadio('bt');
							var re2=/["]BT\s*-n\s*'([^"]+)'/g;
							var m2=re2.exec(m[1]);
							if(m2.length>=2){
								$("#btsinkdiv").val(m2[1]);
							}
						}
					} else if (key == 'host_name') {
						val = val.replaceAll('"', '');
						$("input#dhcp-name1").val(val);
						$("input#dhcp-name2").val(val);
						$("#player").val(val);
						document.title=val;
					}

					$("tbody#nvsTable").append(
						"<tr>" +
						"<td>" + key + "</td>" +
						"<td class='value'>" +
						"<input type='text' class='form-control nvs' id='" + key + "'  nvs_type=" + data[key].type + " >" +
						"</td>" +
						"</tr>"
					);
					$("input#" + key).val(data[key].value);
				}
			});
			$("tbody#nvsTable").append("<tr><td><input type='text' class='form-control' id='nvs-new-key' placeholder='new key'></td><td><input type='text' class='form-control' id='nvs-new-value' placeholder='new value' nvs_type=33 ></td></tr>");
			if (entries.hasOwnProperty('gpio')) {
				$("tbody#gpiotable tr").remove();
				entries.gpio.forEach(function(gpio_entry) {
					cl = gpio_entry.fixed ? "table-secondary" : "table-primary";
					$("tbody#gpiotable").append('<tr class=' + cl + '><th scope="row">' + gpio_entry.group + '</th><td>' + gpio_entry.name + '</td><td>' + gpio_entry.gpio + '</td><td>' + (gpio_entry.fixed ? 'Fixed':'Configuration') + '</td></tr>');
				});
			}
		})
		.fail(function(xhr, ajaxOptions, thrownError) {
			handleExceptionResponse(xhr, ajaxOptions, thrownError);
			blockAjax = false;
		});
}
function showLocalMessage(message,severity, age = 0){
	msg={
		'type':'Local',
		'message':message,
		'type' : severity
	}
	showMessage(msg,severity,age);
}

function showMessage(msg, msg_time,age = 0) {
	color='table-success';
	
	if (msg['type'] == 'MESSAGING_WARNING') {
		color='table-warning';
		if(messageseverity=='MESSAGING_INFO'){
			messageseverity = 'MESSAGING_WARNING';
		}
	} else if (msg['type'] == 'MESSAGING_ERROR') {
		if(messageseverity=='MESSAGING_INFO' || messageseverity=='MESSAGING_WARNING'){
			messageseverity = 'MESSAGING_ERROR';
		}		
		color ='table-danger';
	} 
	if(++messagecount>0){
		$('#msgcnt').removeClass('badge-success');
		$('#msgcnt').removeClass('badge-warning');
		$('#msgcnt').removeClass('badge-danger');
		$('#msgcnt').addClass(pillcolors[messageseverity]);
		$('#msgcnt').text(messagecount);
	}
	
	$("#syslogTable").append(
		"<tr class='" + color + "'>" +
		"<td>" + msg_time.toLocaleString() + "</td>" +
		"<td>" + escapeHTML(msg["message"]).replace(/\n/g, '<br />') + "</td>" +
		"</tr>"
	);
}

function inRange(x, min, max) {
	return ((x - min) * (x - max) <= 0);
}

function sleep(ms) {
	return new Promise(resolve => setTimeout(resolve, ms));
}