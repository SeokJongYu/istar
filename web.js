#!/usr/bin/env node

var fs = require('fs'),
	cluster = require('cluster');
if (cluster.isMaster) {
	// Allocate arrays to hold ligand properties
	var num_ligands = 23129083,
		mwt = new Float32Array(num_ligands),
		lgp = new Float32Array(num_ligands),
		ads = new Float32Array(num_ligands),
		pds = new Float32Array(num_ligands),
		hbd = new Int16Array(num_ligands),
		hba = new Int16Array(num_ligands),
		psa = new Int16Array(num_ligands),
		chg = new Int16Array(num_ligands),
		nrb = new Int16Array(num_ligands);
	// Parse ligand properties
	var prop = 'idock/16_prop.bin.gz';
	console.log('Parsing %s', prop);
	var start = Date.now();
	fs.readFile(prop, function(err, data) {
		if (err) throw err;
		require('zlib').gunzip(data, function(err, buf) {
			if (err) throw err;
			for (i = 0, o = 0; i < num_ligands; ++i, o += 26) {
				mwt[i] = buf.readFloatLE(o +  0);
				lgp[i] = buf.readFloatLE(o +  4);
				ads[i] = buf.readFloatLE(o +  8);
				pds[i] = buf.readFloatLE(o + 12);
				hbd[i] = buf.readInt16LE(o + 16);
				hba[i] = buf.readInt16LE(o + 18);
				psa[i] = buf.readInt16LE(o + 20);
				chg[i] = buf.readInt16LE(o + 22);
				nrb[i] = buf.readInt16LE(o + 24);
			}
			console.log('Parsed %d ligands within %d milliseconds', num_ligands, Date.now() - start);
			// Fork worker processes with cluster
			var numCPUs = require('os').cpus().length;
			console.log('Forking %d worker processes', numCPUs);
			var msg = function(m) {
				if (m.query == '/idock/ligands') {
					var ligands = 0;
					for (var i = 0; i < num_ligands; ++i) {
						if ((m.mwt_lb <= mwt[i]) && (mwt[i] <= m.mwt_ub) && (m.lgp_lb <= lgp[i]) && (lgp[i] <= m.lgp_ub) && (m.ads_lb <= ads[i]) && (ads[i] <= m.ads_ub) && (m.pds_lb <= pds[i]) && (pds[i] <= m.pds_ub) && (m.hbd_lb <= hbd[i]) && (hbd[i] <= m.hbd_ub) && (m.hba_lb <= hba[i]) && (hba[i] <= m.hba_ub) && (m.psa_lb <= psa[i]) && (psa[i] <= m.psa_ub) && (m.chg_lb <= chg[i]) && (chg[i] <= m.chg_ub) && (m.nrb_lb <= nrb[i]) && (nrb[i] <= m.nrb_ub)) ++ligands;
					}
					this.send({ligands: ligands});
				}
			}
			for (var i = 0; i < numCPUs; i++) {
				cluster.fork().on('message', msg);
			}
			cluster.on('death', function(worker) {
				console.error('Worker process %d died. Restarting...', worker.pid);
				cluster.fork().on('message', msg);
			});
		});
	});
} else {
	// Connect to MongoDB
	var mongodb = require('mongodb');
	new mongodb.MongoClient(new mongodb.Server(process.argv[2], 27017), {native_parser: true}).open(function(err, mongoClient) {
		if (err) throw err;
		var db = mongoClient.db('istar');
		db.authenticate(process.argv[3], process.argv[4], function(err, authenticated) {
			if (err) throw err;
			var idock = db.collection('idock');
			var igrow = db.collection('igrow');
			var igrep = db.collection('igrep');
			var usr   = db.collection('usr');
			var usrcat= db.collection('usrcat');
			// Configure express server
			var express = require('express');
			var compress = require('compression');
			var bodyParser = require('body-parser');
			var favicon = require('serve-favicon');
			var errorHandler = require('errorhandler');
			var app = express();
			app.use(compress());
			app.use(bodyParser.urlencoded({ limit: '12mb', extended: false }));
			app.use(errorHandler({ dumpExceptions: true, showStack: true }));
			var env = process.env.NODE_ENV || 'development';
			if (env == 'development') {
				app.use(express.static(__dirname + '/public'));
				app.use(express.static('/home/hjli/nfs/hjli/istar/public'));
				app.use(favicon(__dirname + '/public/favicon.ico'));
			} else if (env == 'production') {
				var oneDay = 1000 * 60 * 60 * 24;
				var oneYear = oneDay * 365.25;
				app.use(express.static(__dirname + '/public', { maxAge: oneDay }));
				app.use(express.static('/home/hjli/nfs/hjli/istar/public', { maxAge: oneDay }));
				app.use(favicon(__dirname + '/public/favicon.ico', { maxAge: oneYear }));
			};
			// Define helper variables and functions
			var child_process = require('child_process');
			var validator = require('./public/validator');
			var ligands;
			function sync(callback) {
				if (ligands == -1) setImmediate(function() {
					sync(callback);
				});
				else callback();
			};
			process.on('message', function(m) {
				if (m.ligands !== undefined) {
					ligands = m.ligands;
				}
			});
			var getJobs = function(req, res, collection, jobFields, progressFields) {
				var v = new validator(req.query);
				if (progressFields === undefined) {
					if (v
						.field('skip').message('must be a non-negative integer').int(0).min(0).copy()
						.failed()) {
						res.json(v.err);
						return;
					};
					collection.find({}, {
						fields: jobFields,
						sort: {'submitted': 1},
						skip: v.res.skip
					}).toArray(function(err, docs) {
						if (err) throw err;
						res.json(docs);
					});
				} else {
					if (v
						.field('skip').message('must be a non-negative integer').int(0).min(0).copy()
						.field('count').message('must be a non-negative integer').int(0).min(0).copy()
						.failed() || v
						.range('skip', 'count')
						.failed()) {
						res.json(v.err);
						return;
					};
					collection.count(function(err, count) {
						if (err) throw err;
						if (v
							.field('count').message('must be no greater than ' + count).max(count)
							.failed()) {
							res.json(v.err);
							return;
						}
						collection.find({}, {
							fields: v.res.count == count ? progressFields : jobFields,
							sort: {'submitted': 1},
							skip: v.res.skip,
							limit: count - v.res.skip
						}).toArray(function(err, docs) {
							if (err) throw err;
							res.json(docs);
						});
					});
				}
			};
			var idockJobFields = {
				'description': 1,
				'ligands': 1,
				'submitted': 1,
				'scheduled': 1,
				'done': 1
			};
			var idockProgressFields = {
				'_id': 0,
				'scheduled': 1,
				'done': 1
			};
			for (var i = 0; i < 10; ++i) {
				idockJobFields[i] = 1;
				idockProgressFields[i] = 1;
			}
			app.route('/idock/jobs').get(function(req, res) {
				getJobs(req, res, idock, idockJobFields, idockProgressFields);
			}).post(function(req, res) {
				var v = new validator(req.body);
				if (v
					.field('email').message('must be valid').email().copy()
					.field('description').message('must be provided, at most 20 characters').length(1, 20).xss().copy()
					.field('receptor').message('must conform to PDB specification').length(1, 10485760).receptor()
					.field('center_x').message('must be a decimal within [-999, 999]').float().min(-999).max(999)
					.field('center_y').message('must be a decimal within [-999, 999]').float().min(-999).max(999)
					.field('center_z').message('must be a decimal within [-999, 999]').float().min(-999).max(999)
					.field('size_x').message('must be an integer within [10, 30]').float().min(10).max(30)
					.field('size_y').message('must be an integer within [10, 30]').float().min(10).max(30)
					.field('size_z').message('must be an integer within [10, 30]').float().min(10).max(30)
					.field('mwt_lb').message('must be a decimal within [55, 567]').float(390).min(55).max(567).copy()
					.field('mwt_ub').message('must be a decimal within [55, 567]').float(420).min(55).max(567).copy()
					.field('lgp_lb').message('must be a decimal within [-6, 12]').float(1).min(-6).max(12).copy()
					.field('lgp_ub').message('must be a decimal within [-6, 12]').float(3).min(-6).max(12).copy()
					.field('ads_lb').message('must be a decimal within [-57, 29]').float(0).min(-57).max(29).copy()
					.field('ads_ub').message('must be a decimal within [-57, 29]').float(10).min(-57).max(29).copy()
					.field('pds_lb').message('must be a decimal within [-543, 1]').float(-40).min(-543).max(1).copy()
					.field('pds_ub').message('must be a decimal within [-543, 1]').float(0).min(-543).max(1).copy()
					.field('hbd_lb').message('must be an integer within [0, 20]').int(2).min(0).max(20).copy()
					.field('hbd_ub').message('must be an integer within [0, 20]').int(4).min(0).max(20).copy()
					.field('hba_lb').message('must be an integer within [0, 18]').int(4).min(0).max(18).copy()
					.field('hba_ub').message('must be an integer within [0, 18]').int(6).min(0).max(18).copy()
					.field('psa_lb').message('must be an integer within [0, 317]').int(60).min(0).max(317).copy()
					.field('psa_ub').message('must be an integer within [0, 317]').int(80).min(0).max(317).copy()
					.field('chg_lb').message('must be an integer within [-5, 5]').int(0).min(-5).max(5).copy()
					.field('chg_ub').message('must be an integer within [-5, 5]').int(0).min(-5).max(5).copy()
					.field('nrb_lb').message('must be an integer within [0, 35]').int(4).min(0).max(35).copy()
					.field('nrb_ub').message('must be an integer within [0, 35]').int(6).min(0).max(35).copy()
					.failed() || v
					.range('mwt_lb', 'mwt_ub')
					.range('lgp_lb', 'lgp_ub')
					.range('ads_lb', 'ads_ub')
					.range('pds_lb', 'pds_ub')
					.range('hbd_lb', 'hbd_ub')
					.range('hba_lb', 'hba_ub')
					.range('psa_lb', 'psa_ub')
					.range('chg_lb', 'chg_ub')
					.range('nrb_lb', 'nrb_ub')
					.failed()) {
					res.json(v.err);
					return;
				}
				// Send query to master process
				ligands = -1;
				process.send({
					query: '/idock/ligands',
					mwt_lb: v.res.mwt_lb,
					mwt_ub: v.res.mwt_ub,
					lgp_lb: v.res.lgp_lb,
					lgp_ub: v.res.lgp_ub,
					ads_lb: v.res.ads_lb,
					ads_ub: v.res.ads_ub,
					pds_lb: v.res.pds_lb,
					pds_ub: v.res.pds_ub,
					hbd_lb: v.res.hbd_lb,
					hbd_ub: v.res.hbd_ub,
					hba_lb: v.res.hba_lb,
					hba_ub: v.res.hba_ub,
					psa_lb: v.res.psa_lb,
					psa_ub: v.res.psa_ub,
					chg_lb: v.res.chg_lb,
					chg_ub: v.res.chg_ub,
					nrb_lb: v.res.nrb_lb,
					nrb_ub: v.res.nrb_ub
				});
				sync(function() {
					if (!(1 <= ligands)) {
						res.json({'ligands': 'the number of filtered ligands must be at least 1'});
						return;
					}
					v.res.ligands = ligands;
					v.res.scheduled = 0;
					v.res.completed = 0;
					for (var i = 0; i < 10; ++i) {
						v.res[i] = 0;
					}
					v.res.submitted = new Date();
					v.res._id = new mongodb.ObjectID();
					var dir = '/home/hjli/nfs/hjli/istar/public/idock/jobs/' + v.res._id;
					fs.mkdir(dir, function (err) {
						if (err) throw err;
						fs.writeFile(dir + '/receptor.pdb', req.body['receptor'], function(err) {
							if (err) throw err;
							child_process.execFile('python2.5', [process.env.HOME + '/mgltools_x86_64Linux2_1.5.6/MGLToolsPckgs/AutoDockTools/Utilities24/prepare_receptor4.pyo', '-A', 'checkhydrogens', '-U', 'nphs_lps_waters_deleteAltB', '-r', 'receptor.pdb'], { cwd: dir }, function(err, stdout, stderr) {
								if (err) {
									fs.unlink(dir + '/receptor.pdb', function(err) {
										if (err) throw err;
										fs.rmdir(dir, function (err) {
											if (err) throw err;
											res.json({ receptor: 'failed PDB to PDBQT conversion' });
										});
									});
								} else {
									fs.writeFile(dir + '/box.conf', ['center_x', 'center_y', 'center_z', 'size_x', 'size_y', 'size_z'].map(function(key) {
										return key + '=' + req.body[key] + '\n';
									}).join(''), function(err) {
										if (err) throw err;
										idock.insert(v.res, { w: 0 });
										res.json({});
									});
								}
							});
						});
					});
				});
			});
			// Get the number of ligands satisfying filtering conditions
			app.route('/idock/ligands').get(function(req, res) {
				// Validate and sanitize user input
				var v = new validator(req.query);
				if (v
					.field('mwt_lb').message('must be a decimal within [55, 567]').float().min(55).max(567).copy()
					.field('mwt_ub').message('must be a decimal within [55, 567]').float().min(55).max(567).copy()
					.field('lgp_lb').message('must be a decimal within [-6, 12]').float().min(-6).max(12).copy()
					.field('lgp_ub').message('must be a decimal within [-6, 12]').float().min(-6).max(12).copy()
					.field('ads_lb').message('must be a decimal within [-57, 29]').float().min(-57).max(29).copy()
					.field('ads_ub').message('must be a decimal within [-57, 29]').float().min(-57).max(29).copy()
					.field('pds_lb').message('must be a decimal within [-543, 1]').float().min(-543).max(1).copy()
					.field('pds_ub').message('must be a decimal within [-543, 1]').float().min(-543).max(1).copy()
					.field('hbd_lb').message('must be an integer within [0, 20]').int().min(0).max(20).copy()
					.field('hbd_ub').message('must be an integer within [0, 20]').int().min(0).max(20).copy()
					.field('hba_lb').message('must be an integer within [0, 18]').int().min(0).max(18).copy()
					.field('hba_ub').message('must be an integer within [0, 18]').int().min(0).max(18).copy()
					.field('psa_lb').message('must be an integer within [0, 317]').int().min(0).max(317).copy()
					.field('psa_ub').message('must be an integer within [0, 317]').int().min(0).max(317).copy()
					.field('chg_lb').message('must be an integer within [-5, 5]').int().min(-5).max(5).copy()
					.field('chg_ub').message('must be an integer within [-5, 5]').int().min(-5).max(5).copy()
					.field('nrb_lb').message('must be an integer within [0, 35]').int().min(0).max(35).copy()
					.field('nrb_ub').message('must be an integer within [0, 35]').int().min(0).max(35).copy()
					.failed() || v
					.range('mwt_lb', 'mwt_ub')
					.range('lgp_lb', 'lgp_ub')
					.range('ads_lb', 'ads_ub')
					.range('pds_lb', 'pds_ub')
					.range('hbd_lb', 'hbd_ub')
					.range('hba_lb', 'hba_ub')
					.range('psa_lb', 'psa_ub')
					.range('chg_lb', 'chg_ub')
					.range('nrb_lb', 'nrb_ub')
					.failed()) {
					res.json(v.err);
					return;
				}
				// Send query to master process
				ligands = -1;
				v.res.query = '/idock/ligands';
				process.send(v.res);
				sync(function() {
					res.json(ligands);
				});
			});
			// Get a specific idock job
			app.route('/idock/job').get(function(req, res) {
				var v = new validator(req.query);
				if (v
					.field('id').message('must be a valid object id').objectid().copy()
					.failed()) {
					res.json(v.err);
					return;
				};
				idock.findOne({
					'_id': new mongodb.ObjectID(v.res.id),
					'done': { $exists: 1 },
				}, {
					'_id': 0,
					'description': 1,
				}, function(err, doc) {
					if (err) throw err;
					res.json(doc);
				});
			});
			app.route('/igrow/jobs').get(function(req, res) {
				getJobs(req, res, igrow, {
					'description': 1,
					'idock_id': 1,
					'submitted': 1,
//					'scheduled': 1,
					'done': 1
				}, {
					'_id': 0,
//					'scheduled': 1,
					'done': 1
				});
			}).post(function(req, res) {
				var v = new validator(req.body);
				if (v
					.field('idock_id').message('must be a valid object id').objectid().copy()
					.field('email').message('must be valid').email().copy()
					.field('description').message('must be provided, at most 20 characters').length(1, 20).xss().copy()
					.field('mms_lb').message('must be a decimal within [0, 1000]').float(300).min(0).max(1000).copy()
					.field('mms_ub').message('must be a decimal within [0, 1000]').float(500).min(0).max(1000).copy()
					.field('nrb_lb').message('must be an integer within [0, 35]' ).int(  0).min( 0).max( 35).copy()
					.field('nrb_ub').message('must be an integer within [0, 35]' ).int( 10).min( 0).max( 35).copy()
					.field('hbd_lb').message('must be an integer within [0, 20]' ).int(  0).min( 0).max( 20).copy()
					.field('hbd_ub').message('must be an integer within [0, 20]' ).int(  5).min( 0).max( 20).copy()
					.field('hba_lb').message('must be an integer within [0, 18]' ).int(  0).min( 0).max( 18).copy()
					.field('hba_ub').message('must be an integer within [0, 18]' ).int( 10).min( 0).max( 18).copy()
					.field('nha_lb').message('must be an integer within [1, 100]').int( 20).min( 1).max(100).copy()
					.field('nha_ub').message('must be an integer within [1, 100]').int( 70).min( 1).max(100).copy()
					.field('lgp_lb').message('must be an integer within [-6, 12]').int(-.4).min(-6).max( 12).copy()
					.field('lgp_ub').message('must be an integer within [-6, 12]').int(5.6).min(-6).max( 12).copy()
					.field('psa_lb').message('must be an integer within [0, 300]').int(  0).min( 0).max(300).copy()
					.field('psa_ub').message('must be an integer within [0, 300]').int(140).min( 0).max(300).copy()
					.field('mrf_lb').message('must be an integer within [0, 300]').int( 40).min( 0).max(300).copy()
					.field('mrf_ub').message('must be an integer within [0, 300]').int(130).min( 0).max(300).copy()
					.failed()) {
					res.json(v.err);
					return;
				};
				idock.findOne({
					'_id': new mongodb.ObjectID(v.res.idock_id),
					'done': { $exists: 1 },
				}, {}, function(err, doc) {
					if (err) throw err;
					if (!doc) {
						res.json({'idock_id': 'must be a completed idock job id'});
						return;
					}
					v.res.submitted = new Date();
					igrow.insert(v.res, {w: 0});
					res.json({});
				});
			});
			app.route('/usr/jobs').get(function(req, res) {
				getJobs(req, res, usr, {
					'description': 1,
					'submitted': 1,
					'done': 1
				});
			}).post(function(req, res) {
				var v = new validator(req.body);
				if (v
					.field('email').message('must be valid').email().copy()
					.field('description').message('must be provided, at most 20 characters').length(1, 20).xss().copy()
					.field('ligand').message('must be an array of 12 numerical features').usr().copy()
					.failed()) {
					res.json(v.err);
					return;
				}
				v.res.submitted = new Date();
				usr.insert(v.res, {w: 0});
				res.json({});
			});
			app.route('/usrcat/jobs').get(function(req, res) {
				getJobs(req, res, usrcat, {
					'description': 1,
					'submitted': 1,
					'done': 1
				});
			}).post(function(req, res) {
				var v = new validator(req.body);
				if (v
					.field('email').message('must be valid').email().copy()
					.field('description').message('must be provided, at most 20 characters').length(1, 20).xss().copy()
					.field('format').message('must be mol2, sdf, xyz, pdb, or pdbqt').in(['mol2', 'sdf', 'xyz', 'pdb', 'pdbqt']).copy()
					.field('ligand').message('must be provided and must not exceed 100KB').length(1, 102400)
					.failed()) {
					res.json(v.err);
					return;
				}
				v.res.submitted = new Date();
				v.res._id = new mongodb.ObjectID();
				var dir = '/home/hjli/nfs/hjli/istar/public/usrcat/jobs/' + v.res._id;
				fs.mkdir(dir, function (err) {
					if (err) throw err;
					fs.writeFile(dir + '/ligand.' + v.res.format, req.body['ligand'], function(err) {
						if (err) throw err;
						usrcat.insert(v.res, { w: 0 });
						res.json({});
					});
				});
			});
			app.route('/igrep/jobs').get(function(req, res) {
				getJobs(req, res, igrep, {
					'taxid': 1,
					'submitted': 1,
					'done': 1
				});
			}).post(function(req, res) {
				var v = new validator(req.body);
				if (v
					.field('email').message('must be valid').email().copy()
					.field('taxid').message('must be the taxonomy id of one of the 26 genomes').int().in([13616, 9598, 9606, 9601, 10116, 9544, 9483, 10090, 9913, 9823, 9796, 9615, 9986, 7955, 28377, 9103, 59729, 9031, 3847, 9258, 29760, 15368, 7460, 30195, 7425, 7070]).copy()
					.field('queries').message('must conform to the specifications').length(2, 66000).queries().copy()
					.failed()) {
					res.json(v.err);
					return;
				}
				v.res.submitted = new Date();
				igrep.insert(v.res, {w: 0});
				res.json({});
			});
			// Start listening
			var http_port = 3000, spdy_port = 3443;
			app.listen(http_port);
			require('spdy').createServer(require('https').Server, {
				key: fs.readFileSync(__dirname + '/key.pem'),
				cert: fs.readFileSync(__dirname + '/cert.pem')
			}, app).listen(spdy_port);
			console.log('Worker %d listening on HTTP port %d and SPDY port %d in %s mode', process.pid, http_port, spdy_port, app.settings.env);
		});
	});
}
