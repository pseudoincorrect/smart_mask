//  Sensor Data Database Access
//
//  Description:
//      Enable the access to sensor data database for
//      the usual Insert, delete, etc..

import 'dart:async';
import 'package:smart_mask/src/logic/database/database.dart';
import 'package:smart_mask/src/logic/database/models/sensor_model.dart';

class SensorDataAccess {
  final dbProvider = DatabaseProvider.dbProvider;

  Future<int> createSensorData(SensorData sensorData) async {
    final db = await dbProvider.database;
    var result = db.insert(sensorDataTABLE, sensorData.toDatabaseJson());
    return result;
  }

  Future<List<SensorData>> getSensorData(Sensor sensor,
      {List<DateTime> interval}) async {
    final db = await dbProvider.database;
    List<Map<String, dynamic>> result;
    String query =
        'SELECT * FROM $sensorDataTABLE WHERE sensor = \'${sensorEnumToString(sensor)}\'';

    if (interval != null) {
      query += ' AND ';
      final int dateLowMs = interval[0].millisecondsSinceEpoch;
      final int dateHighMs = interval[1].millisecondsSinceEpoch;
      query += ' timeStamp > $dateLowMs AND timeStamp < $dateHighMs';
    }

    result = await db.rawQuery(query);

    List<SensorData> sensorData =
        result.map((item) => SensorData.fromDatabaseJson(item)).toList();
    return sensorData;
  }

  Future<int> deleteAllSensorData() async {
    final db = await dbProvider.database;
    var result = await db.rawDelete('DELETE FROM $sensorDataTABLE');
    return result;
  }

  Future<int> deleteSensorDataOlderThan(DateTime date) async {
    final db = await dbProvider.database;
    final int dateMs = date.millisecondsSinceEpoch;
    var result = await db
        .rawDelete('DELETE FROM $sensorDataTABLE WHERE timeStamp < $dateMs');
    return result;
  }
}