/*
	Copyright (c) 2012, The Chinese University of Hong Kong

	This program is free software: you can redistribute it and/or mody
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

var iview = (function() {

	function Element(ad, color, covalentRadius) {
		this.ad = ad;
		this.r = parseInt(color.substring(1, 3), 16) / 255.0;
		this.g = parseInt(color.substring(3, 5), 16) / 255.0;
		this.b = parseInt(color.substring(5, 7), 16) / 255.0;
		this.covalentRadius = covalentRadius;
	}

	E = [];
	E['H' ] = new Element( 0, '#FFFFFF', 0.407);
	E['HD'] = new Element( 1, '#FFFFFF', 0.407);
	E['C' ] = new Element( 2, '#909090', 0.847);
	E['A' ] = new Element( 3, '#909090', 0.847);
	E['N' ] = new Element( 4, '#3050F8', 0.825);
	E['NA'] = new Element( 5, '#3050F8', 0.825);
	E['OA'] = new Element( 6, '#FF0D0D', 0.803);
	E['S' ] = new Element( 7, '#FFFF30', 1.122);
	E['SA'] = new Element( 8, '#FFFF30', 1.122);
	E['Se'] = new Element( 9, '#FFA100', 1.276);
	E['P' ] = new Element(10, '#FF8000', 1.166);
	E['F' ] = new Element(11, '#90E050', 0.781);
	E['Cl'] = new Element(12, '#1FF01F', 1.089);
	E['Br'] = new Element(13, '#A62929', 1.254);
	E['I' ] = new Element(14, '#940094', 1.463);
	E['Zn'] = new Element(15, '#7D80B0', 1.441);
	E['Fe'] = new Element(16, '#E06633', 1.375);
	E['Mg'] = new Element(17, '#8AFF00', 1.430);
	E['Ca'] = new Element(18, '#3DFF00', 1.914);
	E['Mn'] = new Element(19, '#9C7AC7', 1.529);
	E['Cu'] = new Element(20, '#C88033', 1.518);
	E['Na'] = new Element(21, '#AB5CF2', 1.694);
	E['K' ] = new Element(22, '#8F40D4', 2.156);
	E['Hg'] = new Element(23, '#B8B8D0', 1.639);
	E['Ni'] = new Element(24, '#50D050', 1.331);
	E['Co'] = new Element(25, '#F090A0', 1.386);
	E['Cd'] = new Element(26, '#FFD98F', 1.628);
	E['As'] = new Element(27, '#BD80E3', 1.309);
	E['Sr'] = new Element(28, '#00FF00', 2.112);

	Atom = function(coord, type) {
		vec3.set(coord, this);
		this.type = type;
		this.isHBD = function() {
			return (this.type == 'HD') || (this.ad >= 15);
		}
		this.isHBA = function() {
			return (this.type == 'NA') || (this.type == 'OA') || (this.type == 'SA');
		}
		this.render = function(gl) {
			var e = E[this.type];
			gl.uniform3f(gl.dUL, e.r, e.g, e.b);
			gl.setModelViewMatrix(mat4.scale(mat4.translate(gl.modelViewMatrix, this, []), [0.3, 0.3, 0.3], []));
			gl.drawElements(gl.TRIANGLES, gl.sphere.vertexIndexBuffer.size, gl.UNSIGNED_SHORT, 0);
		};
	};

	Bond = function(a1, a2) {
		this.a1 = a1;
		this.a2 = a2;
		this.render = function(gl) {
			var ang = 0;
			var axis = [0, 0, 1];
			if (this.a1[0] == this.a2[0] && this.a1[2] == this.a2[2]) {
				if (this.a2[1] < this.a1[1]) {
					ang = Math.PI;
				}
			} else {
				var y = [0, 1, 0];
				var a1m = vec3.scale(vec3.subtract(this.a2, this.a1, []), 0.5, []);
				ang = Math.acos(vec3.dot(y, a1m) / vec3.length(a1m));
				axis = vec3.cross(y, a1m, []);
			}
			var scaleVector = [0.3, vec3.dist(this.a1, this.a2) * 0.5, 0.3];
			// Draw one half.
			var e1 = E[this.a1.type];
			gl.uniform3f(gl.dUL, e1.r, e1.g, e1.b);
			gl.setModelViewMatrix(mat4.scale(mat4.rotate(mat4.translate(gl.modelViewMatrix, this.a1, []), ang, axis, []), scaleVector, []));
			gl.drawArrays(gl.TRIANGLE_STRIP, 0, gl.cylinder.vertexPositionBuffer.size);
			// Draw the other half.
			var e2 = E[this.a2.type];
			gl.uniform3f(gl.dUL, e2.r, e2.g, e2.b);
			gl.setModelViewMatrix(mat4.scale(mat4.rotate(mat4.translate(gl.modelViewMatrix, this.a2, []), ang + Math.PI, axis, []), scaleVector, []));
			gl.drawArrays(gl.TRIANGLE_STRIP, 0, gl.cylinder.vertexPositionBuffer.size);
		};
	};

	HBond = function(a1, a2) {
		this.a1 = a1;
		this.a2 = a2;
		this.render = function(gl) {
			var ang = 0;
			var axis = [0, 0, 1];
			if (this.a1[0] == this.a2[0] && this.a1[2] == this.a2[2]) {
				if (this.a2[1] < this.a1[1]) {
					ang = Math.PI;
				}
			} else {
				var y = [0, 1, 0];
				var a1a2 = vec3.subtract(this.a2, this.a1, []);
				ang = Math.acos(vec3.dot(y, a1a2) / vec3.length(a1a2));
				axis = vec3.cross(y, a1a2, []);
			}
			gl.setModelViewMatrix(mat4.scale(mat4.rotate(mat4.translate(gl.modelViewMatrix, this.a1, []), ang, axis, []), [0.05, vec3.dist(this.a1, this.a2), 0.05], []));
			gl.drawArrays(gl.TRIANGLE_STRIP, 0, gl.cylinder.vertexPositionBuffer.size);
		};
	};

	Molecule = function() {
		this.atoms = [];
		this.bonds = [];
		this.hbds = [];
		this.hbas = [];
	};

	Mesh = function() {
	};
	Mesh.prototype.createBuffers = function(gl, positionData, normalData, indexData) {
		this.vertexPositionBuffer = gl.createBuffer();
		gl.bindBuffer(gl.ARRAY_BUFFER, this.vertexPositionBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positionData), gl.STATIC_DRAW);
		this.vertexPositionBuffer.size = positionData.length / 3;

		this.vertexNormalBuffer = gl.createBuffer();
		gl.bindBuffer(gl.ARRAY_BUFFER, this.vertexNormalBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(normalData), gl.STATIC_DRAW);
		this.vertexNormalBuffer.size = normalData.length / 3;
			
		if (indexData) {
			this.vertexIndexBuffer = gl.createBuffer();
			gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.vertexIndexBuffer);
			gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(indexData), gl.STATIC_DRAW);
			this.vertexIndexBuffer.size = indexData.length;
		}
	};
	Mesh.prototype.bindBuffers = function(gl) {
		gl.bindBuffer(gl.ARRAY_BUFFER, this.vertexPositionBuffer);
		gl.vertexAttribPointer(gl.vertexPositionAttribute, 3, gl.FLOAT, false, 0, 0);
		gl.bindBuffer(gl.ARRAY_BUFFER, this.vertexNormalBuffer);
		gl.vertexAttribPointer(gl.vertexNormalAttribute, 3, gl.FLOAT, false, 0, 0);
		if (this.vertexIndexBuffer) {
			gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.vertexIndexBuffer);
		}
	};

	Sphere = function(gl, bands) {
		var positionData = [];
		var normalData = [];
		var latitudeAngle = Math.PI / bands;
		var longitudeAngle = 2 * latitudeAngle;
		for (var latNumber = 0; latNumber <= bands; ++latNumber) {
			var theta = latitudeAngle * latNumber;
			var sinTheta = Math.sin(theta);
			var y = Math.cos(theta);
			for (var longNumber = 0; longNumber <= bands; ++longNumber) {
				var phi = longitudeAngle * longNumber;
				var x = Math.cos(phi) * sinTheta;
				var z = Math.sin(phi) * sinTheta;
				normalData.push(x, y, z);
				positionData.push(x, y, z);
			}
		}
		var indexData = [];
		for (var latNumber = 0; latNumber < bands; ++latNumber) {
			var offset = latNumber * (bands + 1);
			for (var longNumber = 0; longNumber <= bands; ++longNumber) {
				var first = offset + longNumber;
				var second = first + (bands + 1);
				indexData.push(first, second, first + 1);
				if (longNumber < bands) {
					indexData.push(second, second + 1, first + 1);
				}
			}
		}
		this.createBuffers(gl, positionData, normalData, indexData);
	};
	Sphere.prototype = new Mesh();

	Cylinder = function(gl, bands) {
		var positionData = [];
		var normalData = [];
		var angle = 2 * Math.PI / bands;
		for (var i = 0; i <= bands; ++i) {
			var theta = angle * i;
			var cosTheta = Math.cos(theta);
			var sinTheta = Math.sin(theta);
			normalData.push(cosTheta, 0, sinTheta);
			positionData.push(cosTheta, 0, sinTheta);
			normalData.push(cosTheta, 0, sinTheta);
			positionData.push(cosTheta, 1, sinTheta);
		}
		this.createBuffers(gl, positionData, normalData);
	};
	Cylinder.prototype = new Mesh();

	var unitRadius = Math.PI / 180.0;
	var CTRL = false;
	var keydownup = function(e) {
		CTRL = e.ctrlKey;
	};
	$(document).keydown(keydownup).keyup(keydownup);

	var iview = function(options) {
		this.options = $.extend({}, options);
		this.canvas = $('#' + this.options.id);
		var gl = this.canvas.get(0).getContext('experimental-webgl', {
			preserveDrawingBuffer: true
		});
		this.disabled = !gl;
		if (this.disabled) return;
		gl.enable(gl.DEPTH_TEST);
		var vertexShader = gl.createShader(gl.VERTEX_SHADER);
		gl.shaderSource(vertexShader, [
			'attribute vec3 a_vertex_position;',
			'attribute vec3 a_vertex_normal;',
			'uniform vec3 u_diffuse_color;',
			'uniform mat4 u_model_view_matrix;',
			'uniform mat4 u_projection_matrix;',
			'uniform mat3 u_normal_matrix;',
			'varying vec3 v_normal;',
			'varying vec3 v_diffuse;',
			'void main() {',
				'if (length(a_vertex_normal) == 0.0) {',
					'v_normal = a_vertex_normal;',
				'} else {',
					'v_normal = normalize(u_normal_matrix * a_vertex_normal);',
				'}',
				'v_diffuse = u_diffuse_color;',
				'gl_Position = u_projection_matrix * u_model_view_matrix * vec4(a_vertex_position, 1.0);',
			'}'
		].join(''));
		gl.compileShader(vertexShader);
		var fragmentShader = gl.createShader(gl.FRAGMENT_SHADER);
		gl.shaderSource(fragmentShader, [
			'precision mediump float;',
			'varying vec3 v_normal;',
			'varying vec3 v_diffuse;',
			'void main() {',
				'if (length(v_normal) == 0.0) {',
					'gl_FragColor = vec4(v_diffuse, 1.0);',
				'} else {',
					'float d = max(dot(v_normal, vec3(0.1, 0.1, 1)), 0.0);',
					'gl_FragColor = vec4(v_diffuse * d + vec3(0.3, 0.3, 0.3) * pow(d, 32.0), 1.0);',
				'}',
			'}'
		].join(''));
		gl.compileShader(fragmentShader);
		var program = gl.createProgram();
		gl.attachShader(program, vertexShader);
		gl.attachShader(program, fragmentShader);
		gl.linkProgram(program);
		gl.useProgram(program);
		gl.vertexPositionAttribute = gl.getAttribLocation(program, 'a_vertex_position');
		gl.vertexNormalAttribute = gl.getAttribLocation(program, 'a_vertex_normal');
		gl.enableVertexAttribArray(gl.vertexPositionAttribute);
		gl.enableVertexAttribArray(gl.vertexNormalAttribute);
		gl.uniformMatrix4fv(gl.getUniformLocation(program, 'u_projection_matrix'), false, mat4.perspective(45, this.canvas.attr('width') / this.canvas.attr('height'), 0.1, 1000));
		gl.dUL = gl.getUniformLocation(program, 'u_diffuse_color');
		gl.mvUL = gl.getUniformLocation(program, 'u_model_view_matrix');
		gl.nUL = gl.getUniformLocation(program, 'u_normal_matrix');
		gl.setModelViewMatrix = function(mvMatrix) {
			this.uniformMatrix4fv(this.mvUL, false, mvMatrix);
			this.uniformMatrix3fv(this.nUL, false, mat3.transpose(mat4.toInverseMat3(mvMatrix, []), []));
		};
		gl.sphere = new Sphere(gl, 60);
		gl.cylinder = new Cylinder(gl, 60);
		this.gl = gl;
		var me = this;
		this.mousebuttons = [];
		this.canvas.bind("contextmenu", function() {
			return false;
		});
		this.canvas.mousedown(function(e) {
			me.ismousedown = true;
			me.mousebuttons[e.which] = true;
			me.pageX = e.pageX;
			me.pageY = e.pageY;
		});
		this.canvas.mouseup(function(e) {
			me.ismousedown = false;
			me.mousebuttons[e.which] = false;
		});
		this.canvas.mousemove(function(e) {
			if (!me.ismousedown) return;
			var dx = e.pageX - me.pageX;
			var dy = e.pageY - me.pageY;
			me.pageX = e.pageX;
			me.pageY = e.pageY;
			if (CTRL) {
				if (me.mousebuttons[1] && !me.mousebuttons[3]) {
					var rotation = mat4.rotate(mat4.rotate(mat4.identity(), dx * unitRadius, [0, 1, 0]), dy * unitRadius, [1, 0, 0], []);
					for (var i = 0, ii = me.ligand.atoms.length; i < ii; ++i) {
						mat4.multiplyVec3(rotation, me.ligand.atoms[i]);
					}
					me.refreshHBonds();
				} else if (me.mousebuttons[3] && !me.mousebuttons[1]) {
					var translation = mat3.multiply(mat4.toInverseMat3(me.gl.modelViewMatrix, []),  [dx * 0.05, -dy * 0.05, 0], []);
					for (var i = 0, ii = me.ligand.atoms.length; i < ii; ++i) {
						vec3.add(me.ligand.atoms[i], translation);
					}
					me.refreshHBonds();
				}
			} else {
				if (me.mousebuttons[1] && !me.mousebuttons[3]) {
					mat4.multiply(mat4.rotate(mat4.rotate(mat4.identity(), dx * unitRadius, [0, 1, 0]), dy * unitRadius, [1, 0, 0], []), me.rotationMatrix, me.rotationMatrix);
				} else if (me.mousebuttons[3] && !me.mousebuttons[1]) {
					mat4.translate(me.translationMatrix, [dx * 0.05, -dy * 0.05, 0]);
				}
			}
			me.repaint();
		});
		this.canvas.mousewheel(function(e, delta) {
			e.preventDefault();
			mat4.translate(me.translationMatrix, [0, 0, delta * 2.5]);
			me.repaint();
		});
	};
	iview.prototype.setBox = function(center, size) {
		this.center = center;
		this.size = size;
		var half = vec3.scale(size, 0.5, []);
		this.corner1 = vec3.subtract(center, half, []);
		this.corner2 = vec3.add(center, half, []);
		this.translationMatrix = mat4.translate(mat4.identity(), [0, 0, -20]);
		this.rotationMatrix = mat4.identity();
	}
	iview.prototype.parseReceptor = function(content) {
		var residues = [], atoms = [];
		for (var residue = 'XXXX', lines = content.split('\n'), ii = lines.length, i = 0; i < ii; ++i) {
			var line = lines[i];
			if (line.match('^ATOM|HETATM')) {
				if ((line[25] != residue[3]) || (line[24] != residue[2]) || (line[23] != residue[1]) || (line[22] != residue[0])) {
					residue = line.substring(22, 26);
					residues.push(atoms.length);
				}
				var type = $.trim(line.substring(77, 79));
				if (type === 'H') continue;
				atoms.push(new Atom([parseFloat(line.substring(30, 38)), parseFloat(line.substring(38, 46)), parseFloat(line.substring(46, 54))], type));
			} else if (line.match('^TER')) {
				residue = 'XXXX';
			}
		}
		residues.push(atoms.length);
		this.receptor = new Molecule();
		for (var r = 0, rr = residues.length - 1; r < rr; ++r) {
			var inside = false;
			for (var i = residues[r], ii = residues[r + 1]; i < ii; ++i) {
				var a = atoms[i];
				if ((this.corner1[0] <= a[0]) && (a[0] < this.corner2[0]) && (this.corner1[1] <= a[1]) && (a[1] < this.corner2[1]) && (this.corner1[2] <= a[2]) && (a[2] < this.corner2[2])) {
					inside = true;
					break;
				}
			}
			if (!inside) continue;
			for (var i = residues[r], ii = residues[r + 1]; i < ii; ++i) {
				var a1 = atoms[i];
				this.receptor.atoms.push(a1);
				for (var j = i + 1; j < ii; ++j) {
					var a2 = atoms[j];
					if (vec3.dist(a1, a2) < E[a1.type].covalentRadius + E[a2.type].covalentRadius) {
						this.receptor.bonds.push(new Bond(a1, a2));
					}
				}
			}
		}
		for (var i = 0, ii = this.receptor.atoms.length; i < ii; ++i) {
			var a = this.receptor.atoms[i];
			vec3.subtract(a, this.center);
			if (a.isHBD()) this.receptor.hbds.push(a);
			else if (a.isHBA()) this.receptor.hbas.push(a);
		}
	};
	iview.prototype.parseLigand = function(content) {
		this.ligand = new Molecule();
		var frames = [0], rotorXes = [], rotorYes = [], serials = [];
		for (var lines = content.split('\n'), ii = lines.length, i = 0; i < ii; ++i) {
			var line = lines[i];
			if (line.match('^ATOM|HETATM')) {
				var type = $.trim(line.substring(77, 79));
				if (type === 'H') continue;
				var a = new Atom([parseFloat(line.substring(30, 38)), parseFloat(line.substring(38, 46)), parseFloat(line.substring(46, 54))], type);
				this.ligand.atoms.push(a);
				serials[parseInt(line.substring(6, 11))] = a;
			} else if (line.match('^BRANCH')) {
				frames.push(this.ligand.atoms.length);
				rotorXes.push(parseInt(line.substring( 6, 10)));
				rotorYes.push(parseInt(line.substring(10, 14)));
			}
		}
		frames.push(this.ligand.atoms.length);
		for (var f = 0, ff = frames.length - 1; f < ff; ++f) {
			for (var i = frames[f], ii = frames[f + 1]; i < ii; ++i) {
				var a1 = this.ligand.atoms[i];
				for (var j = i + 1; j < ii; ++j) {
					var a2 = this.ligand.atoms[j];
					if (vec3.dist(a1, a2) < E[a1.type].covalentRadius + E[a2.type].covalentRadius) {
						this.ligand.bonds.push(new Bond(a1, a2));
					}
				}
			}
		}
		for (var i = 0, ii = rotorXes.length; i < ii; ++i) {
			this.ligand.bonds.push(new Bond(serials[rotorXes[i]], serials[rotorYes[i]]));
		}
		for (var i = 0, ii = this.ligand.atoms.length; i < ii; ++i) {
			var a = this.ligand.atoms[i];
			vec3.subtract(a, this.center);
			if (a.isHBD()) this.ligand.hbds.push(a);
			else if (a.isHBA()) this.ligand.hbas.push(a);
		}
		this.refreshHBonds();
	}
	iview.prototype.refreshHBonds = function() {
		this.hbonds = [];
		for (var i = 0, ii = this.receptor.hbds.length; i < ii; ++i) {
			var r = this.receptor.hbds[i];
			for (var j = 0, jj = this.ligand.hbas.length; j < jj; ++j) {
				var l = this.ligand.hbas[j];
				if (vec3.dist(r, l) < 3.5) {
					this.hbonds.push(new HBond(r, l));
				}
			}
		}
		for (var i = 0, ii = this.receptor.hbas.length; i < ii; ++i) {
			var r = this.receptor.hbas[i];
			for (var j = 0, jj = this.ligand.hbds.length; j < jj; ++j) {
				var l = this.ligand.hbds[j];
				if (vec3.dist(r, l) < 3.5) {
					this.hbonds.push(new HBond(r, l));
				}
			}
		}
		if (this.options.ligandmove) this.options.ligandmove(this.hbonds);
	};
	iview.prototype.repaint = function() {
		this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);
		this.gl.modelViewMatrix = mat4.multiply(this.translationMatrix, this.rotationMatrix, []);
		// Draw atoms.
		this.gl.sphere.bindBuffers(this.gl);
		for (var i = 0, ii = this.receptor.atoms.length; i < ii; ++i) {
			this.receptor.atoms[i].render(this.gl);
		}
		for (var i = 0, ii = this.ligand.atoms.length; i < ii; ++i) {
			this.ligand.atoms[i].render(this.gl);
		}
		// Draw covalent bonds.
		this.gl.cylinder.bindBuffers(this.gl);
		for (var i = 0, ii = this.receptor.bonds.length; i < ii; ++i) {
			this.receptor.bonds[i].render(this.gl);
		}
		for (var i = 0, ii = this.ligand.bonds.length; i < ii; ++i) {
			this.ligand.bonds[i].render(this.gl);
		}
		// Draw hydrogen bonds.
		this.gl.uniform3f(this.gl.dUL, 0.2, 1.0, 0.2);
		for (var i = 0, ii = this.hbonds.length; i < ii; ++i) {
			this.hbonds[i].render(this.gl);
		}
	};
	iview.prototype.png = function() {
		window.open(this.canvas.get(0).toDataURL('image/png'));
	};

	return iview;

})();
