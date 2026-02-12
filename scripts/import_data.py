#!/usr/bin/env python3
"""
WaitingTheLongest Data Import Pipeline
Imports shelter and breed data into PostgreSQL database
"""

import argparse
import json
import csv
import os
import sys
from pathlib import Path
from typing import List, Dict, Any, Tuple
import logging

try:
    import psycopg2
    from psycopg2.extras import execute_batch
except ImportError:
    print("Error: psycopg2 is required. Install it with: pip install psycopg2-binary")
    sys.exit(1)


# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class DatabaseConfig:
    """Database configuration from environment variables"""

    def __init__(self):
        self.host = os.getenv('WTL_DB_HOST', 'localhost')
        self.port = int(os.getenv('WTL_DB_PORT', 5432))
        self.dbname = os.getenv('WTL_DB_NAME', 'waitingthelongest')
        self.user = os.getenv('WTL_DB_USER', 'postgres')
        self.password = os.getenv('WTL_DB_PASSWORD', 'postgres')

    def get_connection_string(self) -> str:
        return f"host={self.host} port={self.port} dbname={self.dbname} user={self.user} password={self.password}"

    def __str__(self):
        return f"PostgreSQL at {self.host}:{self.port}/{self.dbname} (user: {self.user})"


class DataImporter:
    """Base class for data importers"""

    BATCH_SIZE = 500
    REPORT_INTERVAL = 100

    def __init__(self, db_config: DatabaseConfig, dry_run: bool = False):
        self.db_config = db_config
        self.dry_run = dry_run
        self.conn = None
        self.cursor = None
        self.stats = {
            'processed': 0,
            'inserted': 0,
            'skipped': 0,
            'errors': 0
        }

    def connect(self):
        """Connect to PostgreSQL database"""
        try:
            self.conn = psycopg2.connect(self.db_config.get_connection_string())
            self.cursor = self.conn.cursor()
            logger.info(f"Connected to {self.db_config}")
        except psycopg2.Error as e:
            logger.error(f"Failed to connect to database: {e}")
            raise

    def disconnect(self):
        """Disconnect from database"""
        if self.cursor:
            self.cursor.close()
        if self.conn:
            self.conn.close()
            logger.info("Disconnected from database")

    def load_json(self, filepath: str) -> List[Dict]:
        """Load JSON file"""
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                return json.load(f)
        except (FileNotFoundError, json.JSONDecodeError) as e:
            logger.error(f"Error loading JSON file {filepath}: {e}")
            raise

    def load_csv(self, filepath: str) -> List[Dict]:
        """Load CSV file"""
        try:
            data = []
            with open(filepath, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                data = list(reader)
            return data
        except (FileNotFoundError, csv.Error) as e:
            logger.error(f"Error loading CSV file {filepath}: {e}")
            raise

    def get_state_id(self, state_abbr: str) -> int:
        """Get state ID from state abbreviation"""
        if not state_abbr:
            return None

        try:
            self.cursor.execute(
                "SELECT id FROM states WHERE abbreviation = %s LIMIT 1",
                (state_abbr.upper(),)
            )
            result = self.cursor.fetchone()
            return result[0] if result else None
        except psycopg2.Error as e:
            logger.warning(f"Error looking up state {state_abbr}: {e}")
            return None

    def report_progress(self, current: int, total: int):
        """Report progress every REPORT_INTERVAL records"""
        if current % self.REPORT_INTERVAL == 0:
            logger.info(
                f"Progress: {current}/{total} processed "
                f"({self.stats['inserted']} inserted, {self.stats['skipped']} skipped, {self.stats['errors']} errors)"
            )

    def print_summary(self, data_type: str):
        """Print import summary"""
        logger.info("=" * 60)
        logger.info(f"Import Summary for {data_type}")
        logger.info("=" * 60)
        logger.info(f"Total Processed: {self.stats['processed']}")
        logger.info(f"Inserted: {self.stats['inserted']}")
        logger.info(f"Skipped: {self.stats['skipped']}")
        logger.info(f"Errors: {self.stats['errors']}")
        if self.dry_run:
            logger.info("DRY RUN MODE - No data was actually written to database")
        logger.info("=" * 60)


class ShelterImporter(DataImporter):
    """Importer for shelter data"""

    def import_shelters(self, filepath: str):
        """Import shelters from JSON file"""
        logger.info(f"Loading shelters from {filepath}")
        data = self.load_json(filepath)

        logger.info(f"Processing {len(data)} shelters...")

        self.connect()
        try:
            for i, record in enumerate(data, 1):
                self.stats['processed'] += 1
                self.report_progress(i, len(data))

                try:
                    self._import_shelter_record(record)
                except Exception as e:
                    logger.error(f"Error importing shelter record {i}: {e}")
                    self.stats['errors'] += 1
                    continue

            if not self.dry_run:
                self.conn.commit()
                logger.info("Changes committed to database")
            else:
                self.conn.rollback()
                logger.info("DRY RUN - changes rolled back")

        finally:
            self.disconnect()

        self.print_summary("Shelters")

    def _import_shelter_record(self, record: Dict[str, Any]):
        """Import a single shelter record"""
        # Extract and validate required fields
        name = record.get('name', '').strip()
        city = record.get('city', '').strip()
        state_abbr = record.get('state', '').strip()

        if not name or not city or not state_abbr:
            logger.warning(f"Skipping incomplete record: {record}")
            self.stats['skipped'] += 1
            return

        # Get state_id
        state_id = self.get_state_id(state_abbr)
        if not state_id:
            logger.warning(f"State '{state_abbr}' not found, skipping shelter: {name}")
            self.stats['skipped'] += 1
            return

        # Prepare insert statement with ON CONFLICT
        insert_query = """
            INSERT INTO shelters (
                name, city, state_id, address, phone, email, website,
                organization_type, ein, is_verified
            ) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            ON CONFLICT (name, city, state_id) DO UPDATE SET
                address = EXCLUDED.address,
                phone = EXCLUDED.phone,
                email = EXCLUDED.email,
                website = EXCLUDED.website,
                organization_type = EXCLUDED.organization_type,
                ein = EXCLUDED.ein,
                updated_at = NOW()
        """

        values = (
            name,
            city,
            state_id,
            record.get('address', '').strip(),
            record.get('phone', '').strip(),
            record.get('email', '').strip(),
            record.get('website', '').strip(),
            record.get('organization_type', 'shelter').strip(),
            record.get('ein', '').strip() or None,
            record.get('is_verified', False)
        )

        if not self.dry_run:
            self.cursor.execute(insert_query, values)

        self.stats['inserted'] += 1


class BreedImporter(DataImporter):
    """Importer for dog breed data"""

    def import_breeds(self, filepath: str):
        """Import dog breeds from JSON file"""
        logger.info(f"Loading dog breeds from {filepath}")
        data = self.load_json(filepath)

        logger.info(f"Processing {len(data)} breeds...")

        self.connect()
        try:
            for i, record in enumerate(data, 1):
                self.stats['processed'] += 1
                self.report_progress(i, len(data))

                try:
                    self._import_breed_record(record)
                except Exception as e:
                    logger.error(f"Error importing breed record {i}: {e}")
                    self.stats['errors'] += 1
                    continue

            if not self.dry_run:
                self.conn.commit()
                logger.info("Changes committed to database")
            else:
                self.conn.rollback()
                logger.info("DRY RUN - changes rolled back")

        finally:
            self.disconnect()

        self.print_summary("Dog Breeds")

    def _import_breed_record(self, record: Dict[str, Any]):
        """Import a single breed record"""
        name = record.get('name', '').strip()

        if not name:
            logger.warning(f"Skipping breed with missing name: {record}")
            self.stats['skipped'] += 1
            return

        insert_query = """
            INSERT INTO dog_breeds (
                name, breed_group, height_imperial, weight_imperial, life_span,
                temperament, energy_level, exercise_needs, grooming_needs,
                good_with_kids, good_with_dogs, good_with_cats,
                apartment_friendly, first_time_owner_friendly, image_url, description
            ) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
            ON CONFLICT (name) DO UPDATE SET
                breed_group = EXCLUDED.breed_group,
                height_imperial = EXCLUDED.height_imperial,
                weight_imperial = EXCLUDED.weight_imperial,
                life_span = EXCLUDED.life_span,
                temperament = EXCLUDED.temperament,
                energy_level = EXCLUDED.energy_level,
                exercise_needs = EXCLUDED.exercise_needs,
                grooming_needs = EXCLUDED.grooming_needs,
                good_with_kids = EXCLUDED.good_with_kids,
                good_with_dogs = EXCLUDED.good_with_dogs,
                good_with_cats = EXCLUDED.good_with_cats,
                apartment_friendly = EXCLUDED.apartment_friendly,
                first_time_owner_friendly = EXCLUDED.first_time_owner_friendly,
                image_url = EXCLUDED.image_url,
                description = EXCLUDED.description
        """

        values = (
            name,
            record.get('breed_group'),
            record.get('height_imperial'),
            record.get('weight_imperial'),
            record.get('life_span'),
            record.get('temperament'),
            record.get('energy_level'),
            record.get('exercise_needs'),
            record.get('grooming_needs'),
            record.get('good_with_kids'),
            record.get('good_with_dogs'),
            record.get('good_with_cats'),
            record.get('apartment_friendly'),
            record.get('first_time_owner_friendly'),
            record.get('image_url'),
            record.get('description')
        )

        if not self.dry_run:
            self.cursor.execute(insert_query, values)

        self.stats['inserted'] += 1


class IRSOrgImporter(DataImporter):
    """Importer for IRS organization data"""

    def import_irs_orgs(self, filepath: str):
        """Import IRS organizations from JSON file"""
        logger.info(f"Loading IRS organizations from {filepath}")
        data = self.load_json(filepath)

        logger.info(f"Processing {len(data)} IRS organizations...")

        self.connect()
        try:
            for i, record in enumerate(data, 1):
                self.stats['processed'] += 1
                self.report_progress(i, len(data))

                try:
                    self._import_irs_org_record(record)
                except Exception as e:
                    logger.error(f"Error importing IRS org record {i}: {e}")
                    self.stats['errors'] += 1
                    continue

            if not self.dry_run:
                self.conn.commit()
                logger.info("Changes committed to database")
            else:
                self.conn.rollback()
                logger.info("DRY RUN - changes rolled back")

        finally:
            self.disconnect()

        self.print_summary("IRS Organizations")

    def _import_irs_org_record(self, record: Dict[str, Any]):
        """Import a single IRS org record"""
        name = record.get('name', '').strip()
        city = record.get('city', '').strip()
        state_abbr = record.get('state', '').strip()

        if not name or not city or not state_abbr:
            logger.warning(f"Skipping incomplete IRS org record: {record}")
            self.stats['skipped'] += 1
            return

        # Get state_id
        state_id = self.get_state_id(state_abbr)
        if not state_id:
            logger.warning(f"State '{state_abbr}' not found, skipping org: {name}")
            self.stats['skipped'] += 1
            return

        insert_query = """
            INSERT INTO shelters (
                name, city, state_id, zip_code, ein, organization_type, is_verified
            ) VALUES (%s, %s, %s, %s, %s, %s, %s)
            ON CONFLICT (name, city, state_id) DO UPDATE SET
                ein = COALESCE(EXCLUDED.ein, shelters.ein),
                zip_code = COALESCE(EXCLUDED.zip_code, shelters.zip_code),
                updated_at = NOW()
        """

        values = (
            name,
            city,
            state_id,
            record.get('zip_code', '').strip() or None,
            record.get('ein', '').strip() or None,
            'rescue',
            record.get('is_verified', False)
        )

        if not self.dry_run:
            self.cursor.execute(insert_query, values)

        self.stats['inserted'] += 1


class CSVShelterImporter(DataImporter):
    """Importer for CSV shelter data"""

    def import_csv_shelters(self, filepath: str):
        """Import shelters from CSV file"""
        logger.info(f"Loading CSV shelters from {filepath}")
        data = self.load_csv(filepath)

        logger.info(f"Processing {len(data)} CSV shelters...")

        self.connect()
        try:
            for i, record in enumerate(data, 1):
                self.stats['processed'] += 1
                self.report_progress(i, len(data))

                try:
                    self._import_csv_shelter_record(record)
                except Exception as e:
                    logger.error(f"Error importing CSV shelter record {i}: {e}")
                    self.stats['errors'] += 1
                    continue

            if not self.dry_run:
                self.conn.commit()
                logger.info("Changes committed to database")
            else:
                self.conn.rollback()
                logger.info("DRY RUN - changes rolled back")

        finally:
            self.disconnect()

        self.print_summary("CSV Shelters")

    def _import_csv_shelter_record(self, record: Dict[str, Any]):
        """Import a single CSV shelter record"""
        name = record.get('Name', '').strip()
        city = record.get('City', '').strip()
        state_abbr = record.get('State', '').strip()

        if not name or not city or not state_abbr:
            logger.warning(f"Skipping incomplete CSV record: {record}")
            self.stats['skipped'] += 1
            return

        # Get state_id
        state_id = self.get_state_id(state_abbr)
        if not state_id:
            logger.warning(f"State '{state_abbr}' not found, skipping shelter: {name}")
            self.stats['skipped'] += 1
            return

        insert_query = """
            INSERT INTO shelters (
                name, city, state_id, address, zip_code
            ) VALUES (%s, %s, %s, %s, %s)
            ON CONFLICT (name, city, state_id) DO UPDATE SET
                address = EXCLUDED.address,
                zip_code = EXCLUDED.zip_code,
                updated_at = NOW()
        """

        values = (
            name,
            city,
            state_id,
            record.get('Address', '').strip(),
            record.get('Zip', '').strip() or None
        )

        if not self.dry_run:
            self.cursor.execute(insert_query, values)

        self.stats['inserted'] += 1


def main():
    parser = argparse.ArgumentParser(
        description='WaitingTheLongest Data Import Pipeline',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python import_data.py --type shelters --file data/master_shelter_database.json
  python import_data.py --type breeds --file data/dog_breeds.json
  python import_data.py --type irs_orgs --file data/irs_animal_orgs.json
  python import_data.py --type csv_shelters --file data/US_Animal_Shelters_Complete_List.csv --dry-run
        """
    )

    parser.add_argument(
        '--type',
        required=True,
        choices=['shelters', 'breeds', 'irs_orgs', 'csv_shelters'],
        help='Type of data to import'
    )
    parser.add_argument(
        '--file',
        required=True,
        help='Path to data file (JSON or CSV)'
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Preview import without writing to database'
    )
    parser.add_argument(
        '--host',
        help='Database host (default: from WTL_DB_HOST or localhost)'
    )
    parser.add_argument(
        '--port',
        type=int,
        help='Database port (default: from WTL_DB_PORT or 5432)'
    )
    parser.add_argument(
        '--dbname',
        help='Database name (default: from WTL_DB_NAME or waitingthelongest)'
    )
    parser.add_argument(
        '--user',
        help='Database user (default: from WTL_DB_USER or postgres)'
    )
    parser.add_argument(
        '--password',
        help='Database password (default: from WTL_DB_PASSWORD or postgres)'
    )

    args = parser.parse_args()

    # Validate file exists
    if not Path(args.file).exists():
        logger.error(f"File not found: {args.file}")
        sys.exit(1)

    # Override config with command-line arguments if provided
    db_config = DatabaseConfig()
    if args.host:
        db_config.host = args.host
    if args.port:
        db_config.port = args.port
    if args.dbname:
        db_config.dbname = args.dbname
    if args.user:
        db_config.user = args.user
    if args.password:
        db_config.password = args.password

    if args.dry_run:
        logger.warning("DRY RUN MODE - No changes will be written to database")

    try:
        if args.type == 'shelters':
            importer = ShelterImporter(db_config, args.dry_run)
            importer.import_shelters(args.file)

        elif args.type == 'breeds':
            importer = BreedImporter(db_config, args.dry_run)
            importer.import_breeds(args.file)

        elif args.type == 'irs_orgs':
            importer = IRSOrgImporter(db_config, args.dry_run)
            importer.import_irs_orgs(args.file)

        elif args.type == 'csv_shelters':
            importer = CSVShelterImporter(db_config, args.dry_run)
            importer.import_csv_shelters(args.file)

    except Exception as e:
        logger.error(f"Import failed: {e}")
        sys.exit(1)

    logger.info("Import completed successfully")


if __name__ == '__main__':
    main()
