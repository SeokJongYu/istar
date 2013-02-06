$(function() {
	var iv = new iview('iview');
	$.get('receptor.pdbqt', function (src) {
		iv.loadReceptor(src);
//		iv.options.background = 'grey';
//		iv.rebuildScene();
//		iv.show();
	});

	function loadFile() {
		var file = $('#file').get(0);
		if (file) file = file.files;
		if (!file || !window.FileReader || !file[0]) {
			alert("No file is selected. Or File API is not supported in your browser. Please try Firefox or Chrome.");
			return;
		}
		var reader = new FileReader();
		reader.onload = function () {
			iv.loadMolecule(reader.result);
		};
		reader.readAsText(file[0]);
	}
		
	function resetView() {
		iv.resetView();
	}

	function saveImage() {
		iv.show();
		window.open(iv.renderer.domElement.toDataURL('image/png'));
	}
});
