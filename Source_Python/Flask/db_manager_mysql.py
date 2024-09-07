import os
import json
import logging
from sqlalchemy import create_engine, Column, Integer, Float, String, Date, ForeignKey
from sqlalchemy.orm import declarative_base, relationship, sessionmaker
from sqlalchemy.exc import SQLAlchemyError
import pymysql
from pymysql.err import OperationalError

# Configuring logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class FieldNotFoundError(Exception):
    """Exception raised when a field is not found in the database."""
    pass

from sqlalchemy.sql import func

def get_field_progress(field_id):
    session = get_session()
    try:
        # Query to obtain field information and seeded cell count.
        result = session.query(
            Field.f_length,
            Field.f_width,
            Field.square_size,
            func.count(Cell.sowed).label('sowed_cells')
        ).outerjoin(Cell, Field.id == Cell.field_id
        ).filter(Field.id == field_id
        ).group_by(Field.id).first()

        if not result:
            raise FieldNotFoundError(f"Field with ID {field_id} not found")

        field_length, field_width, square_size, sowed_cells = result

        # Calculating the total number of cells.        
        cells_per_row = int(field_length / square_size)
        cells_per_col = int(field_width / square_size)
        total_cells = cells_per_row * cells_per_col

        if total_cells == 0:
            raise ValueError("Total number of cells cannot be zero.")

        progress_percentage = (sowed_cells / total_cells) * 100

        logger.info(f"Field ID {field_id} - Total cells: {total_cells}, Sowed cells: {sowed_cells}, Progress: {progress_percentage:.2f}%")
        
        return {
            'total_cells': total_cells,
            'sowed_cells': sowed_cells,
            'progress_percentage': progress_percentage
        }

    except FieldNotFoundError as fnf_error:
        logger.error(str(fnf_error))
        return None
    except ValueError as ve:
        logger.error(f"Value Error: {str(ve)}")
        return None
    except SQLAlchemyError as e:
        logger.error(f"Error retrieving field progress: {str(e)}")
        return None
    finally:
        close_session(session)


# Connection configuration
config = {
    'host': 'localhost',
    'user': 'root',
    'password': 'root',  
    'charset': 'utf8mb4',
    'cursorclass': pymysql.cursors.DictCursor
}



# Configuration for MySQL
DATABASE_URL = "mysql+pymysql://seedbot:seedbot24@localhost/seedbot"

Base = declarative_base()

# Defining models
class Device(Base):
    __tablename__ = 'Device'
    name = Column(String(255), primary_key=True)
    ipv6_address = Column(String(100))

class Field(Base):
    __tablename__ = 'Field'
    id = Column(Integer, primary_key=True, autoincrement=True)
    f_length = Column(Float, nullable=False)
    f_width = Column(Float, nullable=False)
    square_size = Column(Float, nullable=False)
    start_sowing_date = Column(Date, nullable=False)
    end_sowing_date = Column(Date, nullable=True)
    sowing_status = Column(String(255), nullable=True)
    cells = relationship('Cell', back_populates='field')

class npk:
    def __init__(self, n=None, p=None, k=None):
        self.n = n
        self.p = p
        self.k = k

    def __repr__(self):
        return f"npk(N={self.n}, P={self.p}, K={self.k})"

    def to_dict(self):
        return {"n": self.n, "p": self.p, "k": self.k}

    def to_json(self):
        return json.dumps(self.to_dict())

    @classmethod
    def from_dict(cls, data):
        return cls(n=data.get("n"), p=data.get("p"), k=data.get("k"))

    @classmethod
    def from_json(cls, json_str):
        data = json.loads(json_str)
        return cls.from_dict(data)

class Cell(Base):
    __tablename__ = 'Cell'
    field_id = Column(Integer, ForeignKey('Field.id'), primary_key=True)
    c_row = Column(Integer, primary_key=True)
    c_col = Column(Integer, primary_key=True)
    n = Column(Float, nullable=True)
    p = Column(Float, nullable=True)
    k = Column(Float, nullable=True)
    moisture = Column(Float, nullable=True)
    ph = Column(Float, nullable=True)
    temperature = Column(Float, nullable=True)
    sowed = Column(Integer, nullable=True)
    field = relationship('Field', back_populates='cells')

def create_user_and_db():
    connection = None
    try:
        # Connecting to MySQL
        connection = pymysql.connect(**config)
        
        with connection.cursor() as cursor:
            # Create user 'seedbot' if it does not exist.
            create_user_query = """
            CREATE USER IF NOT EXISTS 'seedbot'@'localhost' IDENTIFIED WITH mysql_native_password BY 'seedbot24';
            """
            cursor.execute(create_user_query)
            
            # Create the 'seedbot' database if it does not exist.
            create_db_query = """
            CREATE DATABASE IF NOT EXISTS seedbot;
            """
            cursor.execute(create_db_query)
            
            # Grant all privileges to user 'seedbot' on database 'seedbot'.
            grant_privileges_query = """
            GRANT ALL PRIVILEGES ON seedbot.* TO 'seedbot'@'localhost' WITH GRANT OPTION;
            """
            cursor.execute(grant_privileges_query)
            
            # Apply the changes
            cursor.execute("FLUSH PRIVILEGES;")
            
            print("User 'seedbot' created, database 'seedbot' created, and privileges granted successfully.")
    
    except OperationalError as e:
        print(f"An error occurred: {e}")
    
    finally:
        if connection:
            connection.close()



def create_tables():
    try:
        engine = create_engine(DATABASE_URL)
        
        # Creazione delle tabelle
        Base.metadata.create_all(engine)
        logger.info(f"Tables successfully created in the database '{engine.url.database}'.")
        
    except SQLAlchemyError as e:
        logger.error(f"Error during the creation of tables: {str(e)}")

def get_session():
    engine = create_engine(DATABASE_URL)
    Session = sessionmaker(bind=engine)
    return Session()

def close_session(session):
    session.close()

def add_device(name, ipv6_address):
    session = get_session()
    try:
        existing_device = session.query(Device).filter_by(name=name).first()
        if existing_device:
            existing_device.ipv6_address = ipv6_address
            session.commit()
            logger.info(f"Updated device with name '{name}'.")
            return 2  # Code for updated existing device
        else:
            new_device = Device(name=name, ipv6_address=ipv6_address)
            session.add(new_device)
            session.commit()
            logger.info(f"Added new device with name '{name}'.")
            return 1  # Code for added new device
    except SQLAlchemyError as e:
        session.rollback()
        logger.error(f"Error adding or updating device: {str(e)}")
        raise e
    finally:
        close_session(session)

def get_all_devices():
    session = get_session()
    devices_dict = {}
    try:
        devices = session.query(Device).all()
        for device in devices:
            devices_dict[device.name] = {
                "ipv6_address": device.ipv6_address
            }
    except SQLAlchemyError as e:
        logger.error(f"Error retrieving devices: {str(e)}")
    finally:
        close_session(session)
    return devices_dict

def get_sensor_by_name(sensor_name):
    session = get_session()
    device_dict = None
    try:
        device = session.query(Device).filter_by(name=sensor_name).first()
        if device:
            device_dict = {
                "name": device.name,
                "ipv6_address": device.ipv6_address
            }
    except SQLAlchemyError as e:
        logger.error(f"Error retrieving device by name: {str(e)}")
    finally:
        close_session(session)
    return device_dict

def add_field(f_length, f_width, square_size, start_sowing_date, end_sowing_date=None, sowing_status=None):
    session = get_session()
    try:
        existing_field = session.query(Field).filter_by(start_sowing_date=start_sowing_date, f_length=f_length, f_width=f_width).first()
        
        if existing_field:
            updated = False
            if existing_field.square_size != square_size:
                existing_field.square_size = square_size
                updated = True
            if existing_field.end_sowing_date != end_sowing_date:
                existing_field.end_sowing_date = end_sowing_date
                updated = True
            if existing_field.sowing_status != sowing_status:
                existing_field.sowing_status = sowing_status
                updated = True
            if updated:
                session.commit()
                logger.info(f"Updated existing field with start sowing date '{start_sowing_date}'.")
            else:
                logger.info(f"No changes detected for field with start sowing date '{start_sowing_date}'.")
        else:
            new_field = Field(f_length=f_length, f_width=f_width, square_size=square_size,
                              start_sowing_date=start_sowing_date, end_sowing_date=end_sowing_date,
                              sowing_status=sowing_status)
            session.add(new_field)
            session.commit()
            logger.info(f"Added new field with start sowing date '{start_sowing_date}'.")
            return new_field.id
    except SQLAlchemyError as e:
        logger.error(f"Error adding or updating field: {str(e)}")
        session.rollback()
    finally:
        close_session(session)

def add_cell(field_id, c_row, c_col, npk=None, moisture=None, ph=None, temperature=None, sowed=None):
    session = get_session()
    try:
        existing_cell = session.query(Cell).filter_by(field_id=field_id, c_row=c_row, c_col=c_col).first()
        
        if existing_cell:
            updated = False
            if npk:
                if existing_cell.n != npk.n:
                    existing_cell.n = npk.n
                    updated = True
                if existing_cell.p != npk.p:
                    existing_cell.p = npk.p
                    updated = True
                if existing_cell.k != npk.k:
                    existing_cell.k = npk.k
                    updated = True
            if existing_cell.moisture != moisture:
                existing_cell.moisture = moisture
                updated = True
            if existing_cell.ph != ph:
                existing_cell.ph = ph
                updated = True
            if existing_cell.temperature != temperature:
                existing_cell.temperature = temperature
                updated = True
            if existing_cell.sowed != sowed:
                existing_cell.sowed = sowed
                updated = True
            if updated:
                session.commit()
                logger.info(f"Updated existing cell at (Field ID {field_id}, Row {c_row}, Column {c_col}).")
                return 2
            else:
                logger.info(f"No changes detected for cell at (Field ID {field_id}, Row {c_row}, Column {c_col}).")
                return 0
        else:
            new_cell = Cell(field_id=field_id, c_row=c_row, c_col=c_col, n=npk.n if npk else None, 
                            p=npk.p if npk else None, k=npk.k if npk else None, moisture=moisture, ph=ph, 
                            temperature=temperature, sowed=sowed)
            session.add(new_cell)
            session.commit()
            logger.info(f"Added new cell at (Field ID {field_id}, Row {c_row}, Column {c_col}).")
            return 1
    except SQLAlchemyError as e:
        logger.error(f"Error adding or updating cell: {str(e)}")
        session.rollback()
    finally:
        close_session(session)

def create_database_and_tables():
    create_user_and_db()
    create_tables()