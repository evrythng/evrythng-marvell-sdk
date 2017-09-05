var provdata;
var provdataview;

attachProvBtnCB = function(provdata, what) {
    var label;
	if (what === "reset_to_prov") {
		label = "Reset to Provisioning";
		$("#prov-btn").unbind();
		$("#prov-btn").bind('click', function() {
			provdataview = prov_show(provdata, "reset_to_prov");
		});
	} else if (what === "prov") {
		label = "Provisioning";
		$("#prov-btn").unbind();
		$("#prov-btn").bind('click', function() {
            provdataview = prov_show(provdata, "prov");
		});
	} else {
		return;
	}
	$("#div-prov-btn ul li a").html(label);
	$("#div-prov-btn").show();
}

var createHTMLPage = function() {
	$("#header #back").hide();
	$("#header #home").hide();
	$("#page_content").html("");
	$("#header #title").text("Marvell Provisioning");
	$("#page_content").append($("<div/>", {"id":"div-prov-btn"})
			.append($("<ul/>", {"data-role":"listview","data-inset":true, "data-divider-theme":"d", id: "ul-prov-btn"})
			.append($("<li/>", {"data-role":"fieldcontain", "data-theme":"c"})
			.append($("<a/>", {"href":"#", "id":"prov-btn", "data-theme":"c", "data-shadow":"false"})
			.append("")))));    
	$("#div-prov-btn").hide();
};

var show = function() {
	createHTMLPage(); 
	
	provdata = prov_init();
	provdata.changedState.attach(function(obj, state) {
		if (obj.state === "unconfigured") {
			attachProvBtnCB(provdata, "prov");
		}
		else if (obj.state === "configured") {
			attachProvBtnCB(provdata, "reset_to_prov");
		}
	});
}

var pageLoad = function() {

        commonInit();
	$("#home").bind("click", function() {
		provdataview.hide();
		provdata.destroy();
		show();
	});

	show();
}
