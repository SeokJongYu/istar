var validator = require('./validator');
var v = new validator.Validator();
var f = new validator.Filter();
var mongodb = require('mongodb');
var db = new mongodb.Db('istar', new mongodb.Server('137.189.90.124', 27017));
var username = 'daemon';
var password = '2qR8dVM9d';

// Get jobs by email
exports.get = function(query, cb) {
  v.init(query).chk('email', 'must be valid', true).isEmail();
  if (Object.keys(v.err).length) {
    return v.err;
  }
  db.open(function(err, db) {
    if (err) throw err;
    db.authenticate(username, password, function(err, res) {
      if (err) throw err;
      db.collection('jobs', function(err, coll) {
        if (err) throw err;
        coll.find({'email': query['email']}, function(err, cursor) {
          if (err) throw err;
          cursor.toArray(function (err, docs) {
            db.close();
            cb(docs);
          });
        });
      });
    });
  });
}

// Create a new job
exports.create = function(job) {
  v.init(job)
   .chk('receptor', 'must be provided', true).len(1, 10485760) // 10MB
   .chk('center_x', 'must be a decimal within [-1000, 1000]', true).isDecimal().min(-1000).max(1000)
   .chk('center_y', 'must be a decimal within [-1000, 1000]', true).isDecimal().min(-1000).max(1000)
   .chk('center_z', 'must be a decimal within [-1000, 1000]', true).isDecimal().min(-1000).max(1000)
   .chk('size_x', 'must be an integer within [1, 30]', true).isInt().min(1).max(30)
   .chk('size_y', 'must be an integer within [1, 30]', true).isInt().min(1).max(30)
   .chk('size_z', 'must be an integer within [1, 30]', true).isInt().min(1).max(30)
   .chk('description', 'must be provided', true).len(1, 1000)
   .chk('email', 'must be valid', true).isEmail()
   .chk('mwt_lb', 'must be a decimal within [55, 566]', false).isDecimal().min(55).max(566)
   .chk('mwt_ub', 'must be a decimal within [55, 566]', false).isDecimal().min(55).max(566)
   .chk('logp_lb', 'must be a decimal within [-6, 12]', false).isDecimal().min(-6).max(12)
   .chk('logp_ub', 'must be a decimal within [-6, 12]', false).isDecimal().min(-6).max(12)
   .chk('nrb_lb', 'must be an integer within [0, 34]', false).isInt().min(0).max(34)
   .chk('nrb_ub', 'must be an integer within [0, 34]', false).isInt().min(0).max(34)
   .chk('hbd_lb', 'must be an integer within [0, 20]', false).isInt().min(0).max(20)
   .chk('hbd_ub', 'must be an integer within [0, 20]', false).isInt().min(0).max(20)
   .chk('hba_lb', 'must be an integer within [0, 18]', false).isInt().min(0).max(18)
   .chk('hba_ub', 'must be an integer within [0, 18]', false).isInt().min(0).max(18)
   .chk('charge_lb', 'must be an integer within [-5, 5]', false).isInt().min(-5).max(5)
   .chk('charge_ub', 'must be an integer within [-5, 5]', false).isInt().min(-5).max(5)
   .chk('ad_lb', 'must be a decimal within [-25, 29]', false).isDecimal().min(-25).max(29)
   .chk('ad_ub', 'must be a decimal within [-25, 29]', false).isDecimal().min(-25).max(29)
   .chk('pd_lb', 'must be a decimal within [-504, 1]', false).isDecimal().min(-504).max(1)
   .chk('pd_ub', 'must be a decimal within [-504, 1]', false).isDecimal().min(-504).max(1)
   .chk('tpsa_lb', 'must be a decimal within [0, 317]', false).isDecimal().min(0).max(317)
   .chk('tpsa_ub', 'must be a decimal within [0, 317]', false).isDecimal().min(0).max(317);
  if (Object.keys(v.err).length) {
    return v.err;
  }
  f.init(job);
  f.snt('receptor').xss();
  f.snt('center_x').toFloat();
  f.snt('center_y').toFloat();
  f.snt('center_z').toFloat();
  f.snt('size_x').toInt();
  f.snt('size_y').toInt();
  f.snt('size_z').toInt();
  f.snt('description').xss();
  f.snt('mwt_lb', 400).toFloat();
  f.snt('mwt_ub', 500).toFloat();
  f.snt('logp_lb', 0).toFloat();
  f.snt('logp_ub', 5).toFloat();
  f.snt('nrb_lb', 2).toInt();
  f.snt('nrb_ub', 8).toInt();
  f.snt('hbd_lb', 2).toInt();
  f.snt('hbd_ub', 5).toInt();
  f.snt('hba_lb', 2).toInt();
  f.snt('hba_ub', 10).toInt();
  f.snt('charge_lb', 0).toInt();
  f.snt('charge_ub', 0).toInt();
  f.snt('ad_lb', 0).toFloat();
  f.snt('ad_ub', 12).toFloat();
  f.snt('pd_lb', -50).toFloat();
  f.snt('pd_ub', 0).toFloat();
  f.snt('tpsa_lb', 20).toFloat();
  f.snt('tpsa_ub', 100).toFloat();
  v.init(f.res);
  v.rng('mwt_lb', 'mwt_ub').rng('logp_lb', 'logp_ub').rng('nrb_lb', 'nrb_ub').rng('hbd_lb', 'hbd_ub').rng('hba_lb', 'hba_ub').rng('charge_lb', 'charge_ub').rng('ad_lb', 'ad_ub').rng('pd_lb', 'pd_ub').rng('tpsa_lb', 'tpsa_ub');
  if (Object.keys(v.err).length) {
    return v.err;
  }
  //job.time = Date.now();
  //job.progress = 0;
  db.open(function(err, db) {
    if (err) throw err;
    db.authenticate(username, password, function(err, res) {
      if (err) throw err;
      db.collection('jobs', function(err, coll) {
        if (err) throw err;
        coll.insert(job, function(err, docs) {
          if (err) throw err;
          db.close();
        });
      });
    });
  });
}
